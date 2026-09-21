#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from typing import Any, Iterable

UPSTREAM_URL = "https://github.com/simdjson/simdjson.git"
UPSTREAM_SHA = "f5de14f09256982933af2849beb43778bd421ca7"
UPSTREAM_LABEL = "v4.6.11"

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[2]

PROFILES = {
    "quick": {
        "corpus_mib": 4,
        "trials": 3,
        "parse_iterations": 30,
        "micro_gib": 0.5,
    },
    "standard": {
        "corpus_mib": 32,
        "trials": 7,
        "parse_iterations": 120,
        "micro_gib": 2.0,
    },
    "publication": {
        "corpus_mib": 64,
        "trials": 11,
        "parse_iterations": 250,
        "micro_gib": 4.0,
    },
}

def say(msg: str = "") -> None:
    print(msg, flush=True)

def run(cmd: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None,
        capture: bool = False, check: bool = True) -> subprocess.CompletedProcess[str]:
    say("> " + " ".join(str(x) for x in cmd))
    return subprocess.run(
        [str(x) for x in cmd],
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        check=check,
    )

def capture(cmd: list[str], *, cwd: Path | None = None) -> str:
    p = run(cmd, cwd=cwd, capture=True)
    return p.stdout.strip()

def command_exists(name: str) -> bool:
    return shutil.which(name) is not None

def git_text(repo: Path, args: list[str]) -> str:
    try:
        return capture(["git", "-C", str(repo), *args])
    except Exception as e:
        return f"<unavailable: {e}>"

def compiler_pair(kind: str) -> tuple[str, str]:
    if kind == "gcc":
        return ("gcc", "g++")
    if kind == "clang":
        return ("clang", "clang++")
    raise ValueError(kind)

def compiler_version(cxx: str) -> str:
    return capture([cxx, "--version"]).splitlines()[0]

def get_virtualization() -> str:
    if command_exists("systemd-detect-virt"):
        p = subprocess.run(["systemd-detect-virt"], text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        out = p.stdout.strip()
        return out or "none"
    return "unknown"

def allowed_core(requested: int | None) -> int:
    try:
        allowed = sorted(os.sched_getaffinity(0))
    except AttributeError:
        allowed = list(range(os.cpu_count() or 1))
    if not allowed:
        return 0
    if requested is None:
        return allowed[0]
    if requested not in allowed:
        raise RuntimeError(f"CPU {requested} is not in allowed affinity set {allowed}")
    return requested

def pinned_prefix(core: int) -> list[str]:
    if command_exists("taskset"):
        return ["taskset", "-c", str(core)]
    return []

def detect_qemu_text() -> bool:
    chunks = []
    for cmd in (["lscpu"], ["cat", "/proc/cpuinfo"]):
        try:
            chunks.append(capture(cmd).lower())
        except Exception:
            pass
    text = "\n".join(chunks)
    return any(token in text for token in ("qemu", "tcg", "virt riscv"))

def compile_and_run_vlen_probe(cxx: str, workspace: Path, compiler: str) -> int:
    src = SCRIPT_DIR / "probe_vlen.cpp"
    out = workspace / "probe_vlen"
    flags = ["-O2", "-std=c++17", "-march=rv64gcv", "-mabi=lp64d"]
    if compiler == "clang":
        flags += ["--gcc-toolchain=/usr"]
    run([cxx, *flags, str(src), "-o", str(out)])
    raw = capture([str(out)])
    try:
        value = int(raw)
    except ValueError:
        raise RuntimeError(f"unexpected VLEN probe output: {raw!r}")
    if value < 128 or value % 8:
        raise RuntimeError(f"implausible RVV VLEN: {value}")
    return value

def preflight(args: argparse.Namespace, workspace: Path) -> dict[str, Any]:
    workspace.mkdir(parents=True, exist_ok=True)
    arch = capture(["uname", "-m"])
    if arch != "riscv64":
        raise RuntimeError(f"native performance requires riscv64; uname -m returned {arch!r}")

    required = ["git", "cmake", "python3"]
    missing = [x for x in required if not command_exists(x)]
    cc, cxx = compiler_pair(args.compiler)
    if not command_exists(cc) or not command_exists(cxx):
        missing.append(f"{cc}/{cxx}")
    if missing:
        raise RuntimeError("missing required tools: " + ", ".join(missing))

    virtualization = get_virtualization()
    qemu_like = detect_qemu_text() or virtualization in {"qemu", "bochs"}
    if qemu_like and not args.allow_emulation:
        raise RuntimeError(
            "QEMU/TCG-like emulation detected. Refusing performance run. "
            "Use real RVV hardware; --allow-emulation is for harness debugging only."
        )

    vlen_bits = compile_and_run_vlen_probe(cxx, workspace, args.compiler)
    core = allowed_core(args.core)

    governor = "unknown"
    gov_path = Path(f"/sys/devices/system/cpu/cpu{core}/cpufreq/scaling_governor")
    if gov_path.exists():
        try:
            governor = gov_path.read_text().strip()
        except OSError:
            pass

    meta = {
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "uname_m": arch,
        "uname_a": capture(["uname", "-a"]),
        "virtualization": virtualization,
        "qemu_like": qemu_like,
        "hardware_vlen_bits": vlen_bits,
        "compiler": args.compiler,
        "cc": cc,
        "cxx": cxx,
        "compiler_version": compiler_version(cxx),
        "core": core,
        "governor": governor,
        "current_git_sha": git_text(REPO_ROOT, ["rev-parse", "HEAD"]),
        "current_git_status": git_text(REPO_ROOT, ["status", "--porcelain=v1"]),
        "upstream_sha": UPSTREAM_SHA,
    }

    (workspace / "metadata.json").write_text(
        json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    for name, cmd in (
        ("lscpu.txt", ["lscpu"]),
        ("cpuinfo.txt", ["cat", "/proc/cpuinfo"]),
    ):
        try:
            (workspace / name).write_text(capture(cmd) + "\n", encoding="utf-8")
        except Exception as e:
            (workspace / name).write_text(f"<unavailable: {e}>\n", encoding="utf-8")

    (workspace / "current_git_status.txt").write_text(
        git_text(REPO_ROOT, ["status", "--porcelain=v1"]) + "\n", encoding="utf-8"
    )
    try:
        diff = capture(["git", "-C", str(REPO_ROOT), "diff", "--binary"])
    except Exception as e:
        diff = f"<unavailable: {e}>"
    (workspace / "current_git_diff.patch").write_text(diff + "\n", encoding="utf-8")

    say("")
    say("Native RVV preflight: PASS")
    say(f"  VLEN:            {vlen_bits} bits")
    say(f"  compiler:        {meta['compiler_version']}")
    say(f"  pinned CPU:      {core}")
    say(f"  governor:        {governor}")
    say(f"  virtualization:  {virtualization}")
    if virtualization not in {"none", "unknown"}:
        say("  WARNING: virtualized host; expect more run-to-run noise than bare metal.")
    if meta["current_git_status"]:
        say("  WARNING: current worktree is dirty; diff is saved in the workspace.")
    return meta

def write_repeated_json(path: Path, item: str, target_bytes: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    item_b = item.encode("utf-8")
    with path.open("wb") as f:
        f.write(b"[")
        first = True
        while f.tell() + len(item_b) + 2 < target_bytes:
            if not first:
                f.write(b",")
            f.write(item_b)
            first = False
        f.write(b"]\n")

def generate_corpus(corpus_dir: Path, mib: int) -> list[Path]:
    corpus_dir.mkdir(parents=True, exist_ok=True)
    target = mib * 1024 * 1024
    specs = {
        "dense_structurals.json":
            '{"a":1,"b":2,"c":[3,4,5],"d":true,"e":null}',
        "numbers.json":
            '-123456789.125e-7',
        "ascii_strings.json":
            '"' + ("abcdefghijklmnopqrstuvwxyz0123456789 " * 7) + '"',
        "unicode_strings.json":
            '"Qu\\u00e9bec-\\u6771\\u4eac-\\u03b5\\u03bb\\u03bb\\u03b7\\u03bd\\u03b9\\u03ba\\u03ac-\\ud83d\\ude80-' +
            ("vector-json-" * 12) + '"',
        "escapes.json":
            '"' + (r'path\\to\\file \"quoted\" line\n tab\t ' * 8) + '"',
    }
    out = []
    for name, item in specs.items():
        p = corpus_dir / name
        if not p.exists() or p.stat().st_size < int(target * 0.9):
            say(f"Generating {name} (~{mib} MiB)")
            write_repeated_json(p, item, target)
        out.append(p)

    for name in ("twitter.json", "citm_catalog.json"):
        src = REPO_ROOT / "jsonexamples" / name
        dst = corpus_dir / name
        if src.exists():
            if not dst.exists() or dst.stat().st_size != src.stat().st_size:
                shutil.copy2(src, dst)
            out.append(dst)

    manifest = {
        p.name: {"bytes": p.stat().st_size, "path": str(p)}
        for p in out
    }
    (corpus_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return out

def prepare_upstream(workspace: Path) -> Path:
    upstream = workspace / "upstream-v4.6.11"
    if not (upstream / ".git").exists():
        run(["git", "clone", "--filter=blob:none", UPSTREAM_URL, str(upstream)])
    run(["git", "-C", str(upstream), "fetch", "--tags", "origin"])
    run(["git", "-C", str(upstream), "checkout", "--detach", UPSTREAM_SHA])
    actual = capture(["git", "-C", str(upstream), "rev-parse", "HEAD"])
    if actual != UPSTREAM_SHA:
        raise RuntimeError(f"upstream checkout mismatch: {actual}")
    return upstream

def cmake_generator() -> list[str]:
    return ["-G", "Ninja"] if command_exists("ninja") else []

def build_flags(compiler: str, mode: str, fixed_bits: int | None = None) -> list[str]:
    flags = ["-O3", "-DNDEBUG", "-mabi=lp64d"]
    if mode == "vla":
        flags += [
            "-march=rv64gcv",
            "-DSIMDJSON_IMPLEMENTATION_RVV=1",
            "-DSIMDJSON_IMPLEMENTATION_RVV_VLS=0",
            "-DSIMDJSON_IMPLEMENTATION_FALLBACK=1",
        ]
    elif mode == "vls":
        if fixed_bits is None:
            raise ValueError("fixed_bits required")
        flags += [
            f"-march=rv64gcv_zvl{fixed_bits}b",
            "-mrvv-vector-bits=zvl",
            "-DSIMDJSON_IMPLEMENTATION_RVV_VLS=1",
            "-DSIMDJSON_IMPLEMENTATION_FALLBACK=1",
        ]
    else:
        raise ValueError(mode)
    if compiler == "clang":
        flags += ["--gcc-toolchain=/usr"]
    return flags

def find_libsimdjson(build: Path) -> Path:
    direct = build / "libsimdjson.a"
    if direct.exists():
        return direct
    matches = list(build.rglob("libsimdjson.a"))
    if len(matches) != 1:
        raise RuntimeError(f"could not uniquely find libsimdjson.a under {build}")
    return matches[0]

def build_one(source: Path, build: Path, bin_dir: Path, label: str,
              compiler: str, mode: str, fixed_bits: int | None = None) -> dict[str, Any]:
    cc, cxx = compiler_pair(compiler)
    flags = build_flags(compiler, mode, fixed_bits)
    flag_string = " ".join(flags)
    build.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["CC"] = cc
    env["CXX"] = cxx

    cmake_cmd = [
        "cmake", "-S", str(source), "-B", str(build),
        *cmake_generator(),
        "-DSIMDJSON_DEVELOPER_MODE=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_C_FLAGS={flag_string}",
        f"-DCMAKE_CXX_FLAGS={flag_string}",
    ]
    run(cmake_cmd, env=env)
    run(["cmake", "--build", str(build), "--target", "parse", "-j", str(os.cpu_count() or 2)], env=env)

    parse_bin = build / "benchmark" / "dom" / "parse"
    if not parse_bin.exists():
        matches = list(build.rglob("parse"))
        matches = [p for p in matches if p.is_file() and os.access(p, os.X_OK)]
        if len(matches) != 1:
            raise RuntimeError(f"parse benchmark not found under {build}")
        parse_bin = matches[0]

    lib = find_libsimdjson(build)
    bin_dir.mkdir(parents=True, exist_ok=True)
    micro = bin_dir / f"rvv_microbench_{label}"
    compile_cmd = [
        cxx, "-std=c++17", *flags,
        "-I", str(source / "include"),
        str(SCRIPT_DIR / "rvv_microbench.cpp"),
        str(lib), "-pthread", "-o", str(micro),
    ]
    run(compile_cmd)

    return {
        "label": label,
        "source": str(source),
        "build": str(build),
        "parse": str(parse_bin),
        "micro": str(micro),
        "flags": flag_string,
        "fixed_bits": fixed_bits,
        "mode": mode,
    }

def prepare(args: argparse.Namespace, workspace: Path, meta: dict[str, Any]) -> dict[str, Any]:
    upstream = prepare_upstream(workspace)
    generate_corpus(workspace / "corpus", args.corpus_mib)

    builds = workspace / "builds"
    bin_dir = workspace / "bin"
    candidates: list[dict[str, Any]] = []

    current = build_one(
        REPO_ROOT, builds / f"current-vla-{args.compiler}", bin_dir,
        "current_vla", args.compiler, "vla"
    )
    candidates.append({**current, "impl": "rvv", "family": "current"})
    candidates.append({
        **current,
        "label": "current_fallback",
        "impl": "fallback",
        "family": "control",
    })

    usable = [x for x in (128, 256, 512) if x <= int(meta["hardware_vlen_bits"])]
    if args.vls_bits:
        requested = [int(x) for x in args.vls_bits.split(",") if x.strip()]
        usable = [x for x in requested if x in (128, 256, 512) and x <= int(meta["hardware_vlen_bits"])]
    if not usable:
        raise RuntimeError("no usable upstream RVV-VLS fixed width for this hardware")

    upstream_first = None
    for bits in usable:
        b = build_one(
            upstream, builds / f"upstream-vls{bits}-{args.compiler}", bin_dir,
            f"upstream_vls_{bits}", args.compiler, "vls", bits
        )
        candidates.append({**b, "impl": "rvv_vls", "family": "upstream"})
        if upstream_first is None:
            upstream_first = b

    if upstream_first is not None:
        candidates.append({
            **upstream_first,
            "label": "upstream_fallback",
            "impl": "fallback",
            "family": "control",
        })

    plan = {
        "upstream_sha": UPSTREAM_SHA,
        "hardware_vlen_bits": meta["hardware_vlen_bits"],
        "compiler": args.compiler,
        "candidates": candidates,
    }
    (workspace / "build_plan.json").write_text(
        json.dumps(plan, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    say("")
    say("Prepare/build: PASS")
    for c in candidates:
        say(f"  {c['label']}: impl={c['impl']} flags={c['flags']}")
    return plan

def parse_tabbed_output(text: str) -> dict[str, float | None]:
    candidates = [line for line in text.splitlines() if "\t" in line and line.lstrip().startswith('"')]
    if not candidates:
        raise RuntimeError("could not find tabbed benchmark row in parse output")
    fields = candidates[-1].split("\t")
    if len(fields) < 8:
        raise RuntimeError(f"unexpected parse tabbed row: {candidates[-1]!r}")
    def f(i: int) -> float | None:
        s = fields[i].strip()
        if not s:
            return None
        return float(s)
    return {
        "alloc_cpb": f(1),
        "stage1_cpb": f(2),
        "stage2_cpb": f(3),
        "all_cpb": f(4),
        "dom_all": f(5),
        "stage1": f(6),
        "stage2": f(7),
    }

def load_plan(workspace: Path) -> dict[str, Any]:
    p = workspace / "build_plan.json"
    if not p.exists():
        raise RuntimeError("build_plan.json missing; run prepare first")
    return json.loads(p.read_text(encoding="utf-8"))

def corpus_files(workspace: Path) -> list[Path]:
    c = workspace / "corpus"
    files = sorted(p for p in c.glob("*.json") if p.name != "manifest.json")
    if not files:
        raise RuntimeError("benchmark corpus is empty; run prepare first")
    return files

RAW_FIELDS = [
    "timestamp_utc", "candidate", "family", "implementation", "fixed_bits",
    "compiler", "flags", "corpus", "bytes", "metric", "trial", "gbps",
    "cycles_per_byte", "iterations", "seconds",
]

def write_raw_header(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        with path.open("w", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=RAW_FIELDS).writeheader()

def append_raw(path: Path, row: dict[str, Any]) -> None:
    with path.open("a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=RAW_FIELDS)
        w.writerow({k: row.get(k, "") for k in RAW_FIELDS})

def candidate_order(candidates: list[dict[str, Any]], trial: int) -> list[dict[str, Any]]:
    # Deterministic rotation + reversal reduces systematic warm/cold ordering bias.
    n = len(candidates)
    if n == 0:
        return []
    shift = trial % n
    order = candidates[shift:] + candidates[:shift]
    if trial % 2:
        order.reverse()
    return order

def run_benchmarks(args: argparse.Namespace, workspace: Path, meta: dict[str, Any]) -> Path:
    plan = load_plan(workspace)
    files = corpus_files(workspace)
    raw = workspace / "results" / "raw.csv"
    if raw.exists() and not args.append:
        raw.unlink()
    write_raw_header(raw)
    core = int(meta["core"])
    prefix = pinned_prefix(core)

    say("")
    say(f"Running {args.trials} trials on CPU {core}")
    for corpus in files:
        size = corpus.stat().st_size
        micro_iterations = max(5, min(
            5000,
            int((args.micro_gib * (1024 ** 3)) / max(1, size))
        ))
        say(f"\n=== {corpus.name} ({size / (1024**2):.1f} MiB) ===")
        for trial in range(args.trials):
            for cand in candidate_order(plan["candidates"], trial):
                common = {
                    "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
                    "candidate": cand["label"],
                    "family": cand["family"],
                    "implementation": cand["impl"],
                    "fixed_bits": cand.get("fixed_bits") or "",
                    "compiler": plan["compiler"],
                    "flags": cand["flags"],
                    "corpus": corpus.name,
                    "bytes": size,
                    "trial": trial + 1,
                }

                parse_cmd = [
                    *prefix, cand["parse"],
                    "-t", "-n", str(args.parse_iterations),
                    "-i", str(min(20, args.parse_iterations)),
                    "-a", cand["impl"],
                    str(corpus),
                ]
                p = run(parse_cmd, capture=True)
                parsed = parse_tabbed_output(p.stdout)
                cpb = {
                    "dom_all": parsed["all_cpb"],
                    "stage1": parsed["stage1_cpb"],
                    "stage2": parsed["stage2_cpb"],
                }
                for metric in ("dom_all", "stage1", "stage2"):
                    append_raw(raw, {
                        **common,
                        "metric": metric,
                        "gbps": parsed[metric],
                        "cycles_per_byte": cpb[metric] if cpb[metric] is not None else "",
                        "iterations": args.parse_iterations,
                        "seconds": "",
                    })

                for metric in ("minify", "utf8"):
                    micro_cmd = [
                        *prefix, cand["micro"],
                        "--impl", cand["impl"],
                        "--mode", metric,
                        "--iterations", str(micro_iterations),
                        "--warmup", "5",
                        "--file", str(corpus),
                    ]
                    p = run(micro_cmd, capture=True)
                    lines = [x for x in p.stdout.splitlines() if x.strip().startswith("{")]
                    if not lines:
                        raise RuntimeError(f"microbench produced no JSON: {p.stdout}")
                    obj = json.loads(lines[-1])
                    append_raw(raw, {
                        **common,
                        "metric": metric,
                        "gbps": obj["gbps"],
                        "cycles_per_byte": "",
                        "iterations": obj["iterations"],
                        "seconds": obj["seconds"],
                    })
                say(f"  trial {trial+1}/{args.trials} {cand['label']}: done")

    say(f"\nRaw results: {raw}")
    return raw

def mad(values: list[float]) -> float:
    med = statistics.median(values)
    return statistics.median([abs(x - med) for x in values])

def geomean(values: Iterable[float]) -> float:
    vals = [v for v in values if v > 0 and math.isfinite(v)]
    if not vals:
        return float("nan")
    return math.exp(sum(math.log(v) for v in vals) / len(vals))

def summarize(workspace: Path) -> tuple[Path, list[dict[str, Any]]]:
    raw = workspace / "results" / "raw.csv"
    if not raw.exists():
        raise RuntimeError("raw.csv missing; run benchmark first")
    groups: dict[tuple[str, str, str], list[dict[str, str]]] = {}
    with raw.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            groups.setdefault((row["candidate"], row["corpus"], row["metric"]), []).append(row)

    out_rows: list[dict[str, Any]] = []
    for (candidate, corpus, metric), rows in sorted(groups.items()):
        vals = [float(r["gbps"]) for r in rows]
        med = statistics.median(vals)
        m = mad(vals)
        first = rows[0]
        out_rows.append({
            "candidate": candidate,
            "family": first["family"],
            "implementation": first["implementation"],
            "fixed_bits": first["fixed_bits"],
            "corpus": corpus,
            "metric": metric,
            "trials": len(vals),
            "median_gbps": med,
            "min_gbps": min(vals),
            "max_gbps": max(vals),
            "mad_gbps": m,
            "mad_percent": (100.0 * m / med) if med else float("nan"),
        })

    summary = workspace / "results" / "summary.csv"
    summary.parent.mkdir(parents=True, exist_ok=True)
    fields = list(out_rows[0].keys()) if out_rows else []
    with summary.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(out_rows)
    return summary, out_rows

def make_report(workspace: Path, meta: dict[str, Any]) -> Path:
    summary_path, rows = summarize(workspace)
    idx = {(r["candidate"], r["corpus"], r["metric"]): r for r in rows}
    corpora = sorted({r["corpus"] for r in rows})
    metrics = ["dom_all", "stage1", "stage2", "minify", "utf8"]
    upstream_candidates = sorted({
        r["candidate"] for r in rows if r["family"] == "upstream"
    })

    lines = []
    lines.append("# RVV-VLA native performance report")
    lines.append("")
    lines.append(f"- UTC: `{meta.get('timestamp_utc', '')}`")
    lines.append(f"- Host: `{meta.get('uname_a', '')}`")
    lines.append(f"- Hardware VLEN: **{meta.get('hardware_vlen_bits', '?')} bits**")
    lines.append(f"- Compiler: `{meta.get('compiler_version', '')}`")
    lines.append(f"- CPU affinity: `{meta.get('core', '')}`")
    lines.append(f"- Governor: `{meta.get('governor', '')}`")
    lines.append(f"- Virtualization: `{meta.get('virtualization', '')}`")
    lines.append(f"- Current commit: `{meta.get('current_git_sha', '')}`")
    lines.append(f"- Upstream reference: `{UPSTREAM_SHA}` ({UPSTREAM_LABEL})")
    lines.append("")
    if meta.get("virtualization") not in ("none", "unknown"):
        lines.append("> Warning: this is a virtualized host. Treat results as VPS-relative; "
                     "repeat on bare metal before making broad hardware performance claims.")
        lines.append("")
    lines.append("Each speedup below uses the **best measured upstream RVV-VLS fixed-width "
                 "variant** for that corpus/metric.")
    lines.append("")

    speedups_by_metric: dict[str, list[float]] = {m: [] for m in metrics}
    for metric in metrics:
        lines.append(f"## {metric}")
        lines.append("")
        lines.append("| Corpus | RVV-VLA GB/s | Best upstream VLS | VLS GB/s | VLA/VLS | VLA MAD% | VLS MAD% |")
        lines.append("|---|---:|---|---:|---:|---:|---:|")
        for corpus in corpora:
            cur = idx.get(("current_vla", corpus, metric))
            ups = [idx.get((c, corpus, metric)) for c in upstream_candidates]
            ups = [u for u in ups if u]
            if not cur or not ups:
                continue
            best = max(ups, key=lambda r: float(r["median_gbps"]))
            ratio = float(cur["median_gbps"]) / float(best["median_gbps"])
            speedups_by_metric[metric].append(ratio)
            lines.append(
                f"| {corpus} | {float(cur['median_gbps']):.3f} | {best['candidate']} | "
                f"{float(best['median_gbps']):.3f} | **{ratio:.3f}x** | "
                f"{float(cur['mad_percent']):.2f}% | {float(best['mad_percent']):.2f}% |"
            )
        gm = geomean(speedups_by_metric[metric])
        if math.isfinite(gm):
            lines.append("")
            lines.append(f"Geometric-mean VLA/VLS speedup across these corpora: **{gm:.3f}x**.")
        lines.append("")

    noisy = [r for r in rows if float(r["mad_percent"]) > 3.0]
    lines.append("## Noise check")
    lines.append("")
    if noisy:
        lines.append(f"{len(noisy)} summary rows have MAD/median > 3%. "
                     "Treat those measurements cautiously; increase trials or move to a quieter CPU.")
    else:
        lines.append("All summary rows are at or below 3% MAD/median.")
    lines.append("")
    lines.append(f"Raw data: `{workspace / 'results' / 'raw.csv'}`")
    lines.append(f"Summary data: `{summary_path}`")
    lines.append("")

    report = workspace / "results" / "report.md"
    report.write_text("\n".join(lines) + "\n", encoding="utf-8")
    say(f"Report: {report}")
    return report

def apply_profile(args: argparse.Namespace) -> None:
    p = PROFILES[args.profile]
    if args.corpus_mib is None:
        args.corpus_mib = p["corpus_mib"]
    if args.trials is None:
        args.trials = p["trials"]
    if args.parse_iterations is None:
        args.parse_iterations = p["parse_iterations"]
    if args.micro_gib is None:
        args.micro_gib = p["micro_gib"]

def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Native RVV performance lab: current RVV-VLA vs pristine simdjson v4.6.11 RVV-VLS."
    )
    p.add_argument("command", choices=("preflight", "prepare", "run", "report", "all"))
    p.add_argument("--workspace", type=Path, default=REPO_ROOT.parent / "rvv-perf-lab")
    p.add_argument("--compiler", choices=("gcc", "clang"), default="gcc")
    p.add_argument("--profile", choices=tuple(PROFILES), default="standard")
    p.add_argument("--core", type=int)
    p.add_argument("--corpus-mib", type=int)
    p.add_argument("--trials", type=int)
    p.add_argument("--parse-iterations", type=int)
    p.add_argument("--micro-gib", type=float)
    p.add_argument("--vls-bits", help="comma-separated subset of 128,256,512; default auto")
    p.add_argument("--append", action="store_true", help="append to existing raw.csv")
    p.add_argument("--allow-emulation", action="store_true",
                   help="debug harness under emulation; results are not valid performance data")
    return p

def main() -> int:
    args = parser().parse_args()
    apply_profile(args)
    workspace = args.workspace.expanduser().resolve()
    workspace.mkdir(parents=True, exist_ok=True)

    if args.command == "report":
        meta_path = workspace / "metadata.json"
        if not meta_path.exists():
            raise RuntimeError("metadata.json missing; run preflight/prepare first")
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
        make_report(workspace, meta)
        return 0

    meta = preflight(args, workspace)

    if args.command == "preflight":
        return 0

    if args.command in ("prepare", "all"):
        prepare(args, workspace, meta)
        if args.command == "prepare":
            return 0

    if args.command in ("run", "all"):
        if not (workspace / "build_plan.json").exists():
            raise RuntimeError("build plan missing; run prepare first")
        run_benchmarks(args, workspace, meta)
        make_report(workspace, meta)
        return 0

    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        raise SystemExit(130)
    except Exception as exc:
        print(f"RVV_PERF_ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)

#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import time
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
PERF_SCRIPT = SCRIPT_DIR / "perf" / "bench_native.py"
TEST_SCRIPT = SCRIPT_DIR / "run_tests.py"

PRESETS = {
    "smoke": {
        "correctness": [("gcc", "global")],
        "acceptance": False,
        "bench_compilers": ["gcc"],
        "bench_profile": "quick",
        "vla_variants": "current",
    },
    "phase1": {
        "correctness": [("gcc", "global"), ("gcc", "dispatch"), ("clang", "global")],
        "acceptance": False,
        "bench_compilers": ["gcc"],
        "bench_profile": "quick",
        "vla_variants": "phase1",
    },
    "matrix": {
        "correctness": [
            ("gcc", "global"), ("gcc", "dispatch"),
            ("clang", "global"), ("clang", "dispatch"),
        ],
        "acceptance": True,
        "bench_compilers": ["gcc", "clang"],
        "bench_profile": "quick",
        "vla_variants": "phase1",
    },
    "standard": {
        "correctness": [
            ("gcc", "global"), ("gcc", "dispatch"),
            ("clang", "global"), ("clang", "dispatch"),
        ],
        "acceptance": True,
        "bench_compilers": ["gcc", "clang"],
        "bench_profile": "standard",
        "vla_variants": "phase1",
    },
}


def utc_stamp() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def iso_utc() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def cmd_text(cmd: list[str]) -> str:
    return " ".join(shlex.quote(str(x)) for x in cmd)


def which(name: str) -> str | None:
    return shutil.which(name)


def capture(cmd: list[str], *, cwd: Path | None = None) -> tuple[int, str]:
    try:
        p = subprocess.run(
            [str(x) for x in cmd], cwd=str(cwd) if cwd else None,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            check=False,
        )
        return p.returncode, p.stdout.strip()
    except Exception as exc:
        return 127, f"<unavailable: {exc}>"


def safe_version(command: list[str]) -> str:
    rc, out = capture(command)
    if rc != 0 or not out:
        return "unavailable"
    return out.splitlines()[0]


def parse_major(text: str) -> int | None:
    m = re.search(r"(?:^|\s)(\d+)(?:\.\d+)", text)
    if not m:
        return None
    return int(m.group(1))


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return ""


def human_seconds(value: float) -> str:
    if value < 60:
        return f"{value:.1f}s"
    minutes, seconds = divmod(value, 60)
    if minutes < 60:
        return f"{int(minutes)}m {seconds:.0f}s"
    hours, minutes = divmod(minutes, 60)
    return f"{int(hours)}h {int(minutes)}m"


class Suite:
    def __init__(self, out_dir: Path, *, verbose: bool = True) -> None:
        self.out_dir = out_dir
        self.verbose = verbose
        self.logs_dir = out_dir / "steps"
        self.env_dir = out_dir / "environment"
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.env_dir.mkdir(parents=True, exist_ok=True)
        self.main_log = out_dir / "suite.log"
        self.steps: list[dict[str, Any]] = []
        self.notes: list[str] = []
        self.environment: dict[str, Any] = {}
        self.preflights: dict[str, Any] = {}
        self.benchmarks: dict[str, Any] = {}
        self.started = time.monotonic()

    def log(self, line: str = "") -> None:
        stamped = f"[{dt.datetime.now().strftime('%H:%M:%S')}] {line}" if line else ""
        if self.verbose:
            print(stamped, flush=True)
        with self.main_log.open("a", encoding="utf-8") as f:
            f.write(stamped + "\n")

    def note(self, text: str) -> None:
        self.notes.append(text)
        self.log("NOTE: " + text)

    def add_step(self, *, name: str, category: str, status: str,
                 duration: float = 0.0, returncode: int | None = None,
                 command: list[str] | None = None, log_path: Path | None = None,
                 required: bool = True, detail: str = "") -> dict[str, Any]:
        row = {
            "name": name,
            "category": category,
            "status": status,
            "required": required,
            "returncode": returncode,
            "duration_seconds": round(duration, 3),
            "command": cmd_text(command) if command else "",
            "log": str(log_path.relative_to(self.out_dir)) if log_path else "",
            "detail": detail,
        }
        self.steps.append(row)
        return row

    def skip(self, name: str, category: str, detail: str, *, required: bool = False) -> None:
        self.log(f"SKIP {name}: {detail}")
        self.add_step(name=name, category=category, status="SKIP", required=required, detail=detail)

    def run_step(self, name: str, category: str, command: list[str], *,
                 cwd: Path = REPO_ROOT, env: dict[str, str] | None = None,
                 required: bool = True) -> dict[str, Any]:
        safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", name).strip("_")
        log_path = self.logs_dir / f"{len(self.steps)+1:02d}_{safe}.log"
        self.log("")
        self.log(f"=== {name} ===")
        self.log("> " + cmd_text(command))
        start = time.monotonic()
        rc = 127
        with log_path.open("w", encoding="utf-8") as lf:
            lf.write("> " + cmd_text(command) + "\n\n")
            try:
                p = subprocess.Popen(
                    [str(x) for x in command], cwd=str(cwd), env=env,
                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    bufsize=1,
                )
                assert p.stdout is not None
                for line in p.stdout:
                    lf.write(line)
                    if self.verbose:
                        print(line, end="", flush=True)
                    with self.main_log.open("a", encoding="utf-8") as mf:
                        mf.write(line)
                rc = p.wait()
            except Exception as exc:
                lf.write(f"\nSUITE_EXEC_ERROR: {exc}\n")
                self.log(f"SUITE_EXEC_ERROR: {exc}")
                rc = 127
        duration = time.monotonic() - start
        status = "PASS" if rc == 0 else "FAIL"
        self.log(f"{status} {name} ({human_seconds(duration)}, rc={rc})")
        return self.add_step(
            name=name, category=category, status=status, duration=duration,
            returncode=rc, command=command, log_path=log_path, required=required,
        )

    def save(self) -> None:
        total = time.monotonic() - self.started
        required_failures = [s for s in self.steps if s["required"] and s["status"] == "FAIL"]
        data = {
            "schema_version": 1,
            "generated_utc": iso_utc(),
            "overall_status": "FAIL" if required_failures else "PASS",
            "duration_seconds": round(total, 3),
            "repo_root": str(REPO_ROOT),
            "environment": self.environment,
            "preflights": self.preflights,
            "steps": self.steps,
            "benchmarks": self.benchmarks,
            "notes": self.notes,
        }
        (self.out_dir / "summary.json").write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        (self.out_dir / "summary.md").write_text(self.make_markdown(data), encoding="utf-8")
        self.make_bundle()

    def make_markdown(self, data: dict[str, Any]) -> str:
        env = data["environment"]
        lines = [
            "# RVV native VPS validation report",
            "",
            f"- Overall: **{data['overall_status']}**",
            f"- Generated UTC: `{data['generated_utc']}`",
            f"- Duration: `{human_seconds(float(data['duration_seconds']))}`",
            f"- Commit: `{env.get('git_sha', 'unknown')}`",
            f"- Git dirty: `{env.get('git_dirty', 'unknown')}`",
            f"- Architecture: `{env.get('machine', 'unknown')}`",
            f"- Virtualization: `{env.get('virtualization', 'unknown')}`",
            f"- QEMU/TCG-like: `{env.get('qemu_like', 'unknown')}`",
            f"- CPUs allowed: `{env.get('affinity', 'unknown')}`",
            f"- GCC: `{env.get('gcc_version', 'unavailable')}`",
            f"- Clang: `{env.get('clang_version', 'unavailable')}`",
            "",
            "## RVV preflight",
            "",
            "| Compiler | Status | VLEN | CPU | Governor |",
            "|---|---|---:|---:|---|",
        ]
        for compiler in ("gcc", "clang"):
            p = data["preflights"].get(compiler)
            if not p:
                continue
            lines.append(
                f"| {compiler} | {p.get('status', '?')} | {p.get('hardware_vlen_bits', '')} | "
                f"{p.get('core', '')} | {p.get('governor', '')} |"
            )
        lines += [
            "",
            "## Steps",
            "",
            "| Step | Category | Status | Required | Duration | Log |",
            "|---|---|---|---|---:|---|",
        ]
        for s in data["steps"]:
            log = f"`{s['log']}`" if s["log"] else ""
            lines.append(
                f"| {s['name']} | {s['category']} | **{s['status']}** | "
                f"{'yes' if s['required'] else 'no'} | {human_seconds(float(s['duration_seconds']))} | {log} |"
            )

        if data["benchmarks"]:
            lines += ["", "## Benchmark summary", ""]
            for compiler, b in data["benchmarks"].items():
                lines.append(f"### {compiler}")
                lines.append("")
                lines.append(f"- Detailed report: `{b.get('report', '')}`")
                lines.append(f"- Raw CSV: `{b.get('raw_csv', '')}`")
                lines.append(f"- Summary CSV: `{b.get('summary_csv', '')}`")
                lines.append(f"- Rows above 3% MAD/median: **{b.get('noisy_rows', '?')}**")
                ratios = b.get("current_vs_best_vls_geomean", {})
                if ratios:
                    lines.append("")
                    lines.append("| Metric | current VLA / best upstream VLS (geomean) |")
                    lines.append("|---|---:|")
                    for metric, ratio in ratios.items():
                        lines.append(f"| {metric} | **{ratio:.3f}x** |")
                ab = b.get("phase1_vs_legacy_geomean", {})
                if ab:
                    lines.append("")
                    lines.append("Phase-1 candidates relative to `vla_legacy` (geomean across corpora):")
                    lines.append("")
                    lines.append("| Metric | Candidate | Relative |")
                    lines.append("|---|---|---:|")
                    for metric in sorted(ab):
                        for candidate, ratio in sorted(ab[metric].items()):
                            lines.append(f"| {metric} | {candidate} | {ratio:.3f}x |")
                lines.append("")

        if data["notes"]:
            lines += ["## Notes", ""]
            for note in data["notes"]:
                lines.append(f"- {note}")
            lines.append("")

        lines += [
            "## Interpretation",
            "",
            "Performance data is valid only when the native preflight passes and QEMU/TCG-like "
            "emulation is not detected. VPS results are comparative results for that host; keep "
            "the raw CSV and metadata with any claim.",
            "",
        ]
        return "\n".join(lines)

    def make_bundle(self) -> None:
        bundle = self.out_dir / "rvv-native-results.tar.gz"
        roots = [self.out_dir / "summary.md", self.out_dir / "summary.json", self.main_log, self.env_dir, self.logs_dir]
        for compiler in ("gcc", "clang"):
            bench = self.out_dir / "benchmarks" / compiler
            for rel in ("metadata.json", "lscpu.txt", "cpuinfo.txt", "current_git_status.txt", "current_git_diff.patch", "build_plan.json"):
                p = bench / rel
                if p.exists():
                    roots.append(p)
            results = bench / "results"
            if results.exists():
                roots.append(results)
        with tarfile.open(bundle, "w:gz") as tf:
            seen: set[Path] = set()
            for root in roots:
                if not root.exists() or root in seen:
                    continue
                seen.add(root)
                tf.add(root, arcname=str(root.relative_to(self.out_dir)))


def detect_virtualization() -> str:
    if which("systemd-detect-virt"):
        rc, out = capture(["systemd-detect-virt"])
        if out:
            return out.splitlines()[-1]
        if rc == 0:
            return "none"
    return "unknown"


def detect_qemu_like() -> bool:
    chunks = []
    for cmd in (["lscpu"], ["cat", "/proc/cpuinfo"]):
        _, out = capture(list(cmd))
        chunks.append(out.lower())
    text = "\n".join(chunks)
    return any(token in text for token in ("qemu", "tcg", "virt riscv"))


def allowed_cpus() -> list[int]:
    try:
        return sorted(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        return list(range(os.cpu_count() or 1))


def compiler_commands(name: str) -> tuple[str, str]:
    if name == "gcc":
        return (
            os.environ.get("RVV_NATIVE_CC", "gcc"),
            os.environ.get("RVV_NATIVE_CXX", "g++"),
        )
    if name == "clang":
        return (
            os.environ.get("RVV_NATIVE_CLANG_CC", "clang"),
            os.environ.get("RVV_NATIVE_CLANG_CXX", "clang++"),
        )
    raise ValueError(name)


def snapshot_environment(suite: Suite) -> None:
    commands = {
        "uname.txt": ["uname", "-a"],
        "lscpu.txt": ["lscpu"],
        "cpuinfo.txt": ["cat", "/proc/cpuinfo"],
        "meminfo.txt": ["cat", "/proc/meminfo"],
        "os-release.txt": ["cat", "/etc/os-release"],
        "df.txt": ["df", "-h", str(REPO_ROOT)],
        "git-status.txt": ["git", "-C", str(REPO_ROOT), "status", "--short", "--branch"],
        "git-log.txt": ["git", "-C", str(REPO_ROOT), "log", "-1", "--decorate=short", "--oneline"],
    }
    for name, cmd in commands.items():
        _, out = capture(cmd)
        (suite.env_dir / name).write_text(out + "\n", encoding="utf-8")

    git_sha_rc, git_sha = capture(["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD"])
    git_status_rc, git_dirty_text = capture(["git", "-C", str(REPO_ROOT), "status", "--porcelain=v1"])
    if git_sha_rc != 0:
        git_sha = "unavailable"
    git_dirty: bool | None = bool(git_dirty_text) if git_status_rc == 0 else None
    virtualization = detect_virtualization()
    qemu_like = detect_qemu_like()
    affinity = allowed_cpus()

    disk = shutil.disk_usage(REPO_ROOT)
    mem_total_kib = 0
    for line in read_text(Path("/proc/meminfo")).splitlines():
        if line.startswith("MemTotal:"):
            try:
                mem_total_kib = int(line.split()[1])
            except (IndexError, ValueError):
                pass
            break

    gcc_cc, gcc_cxx = compiler_commands("gcc")
    clang_cc, clang_cxx = compiler_commands("clang")
    env = {
        "timestamp_utc": iso_utc(),
        "machine": platform.machine(),
        "platform": platform.platform(),
        "virtualization": virtualization,
        "qemu_like": qemu_like,
        "affinity": affinity,
        "cpu_count": os.cpu_count(),
        "git_sha": git_sha,
        "git_dirty": git_dirty,
        "git_dirty_text": git_dirty_text if git_status_rc == 0 else "unavailable",
        "python_version": sys.version.splitlines()[0],
        "cmake_version": safe_version(["cmake", "--version"]),
        "ninja_version": safe_version(["ninja", "--version"]) if which("ninja") else "unavailable",
        "gcc_cc": gcc_cc,
        "gcc_cxx": gcc_cxx,
        "gcc_version": safe_version([gcc_cxx, "--version"]) if which(gcc_cxx) else "unavailable",
        "clang_cc": clang_cc,
        "clang_cxx": clang_cxx,
        "clang_version": safe_version([clang_cxx, "--version"]) if which(clang_cxx) else "unavailable",
        "taskset": which("taskset") or "unavailable",
        "perf": which("perf") or "unavailable",
        "disk_free_gib": round(disk.free / (1024 ** 3), 2),
        "memory_gib": round(mem_total_kib / (1024 ** 2), 2) if mem_total_kib else None,
    }
    suite.environment = env
    (suite.env_dir / "environment.json").write_text(
        json.dumps(env, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    suite.log("Machine snapshot:")
    suite.log(f"  arch={env['machine']} virtualization={virtualization} qemu_like={qemu_like}")
    suite.log(f"  cpus={affinity} disk_free={env['disk_free_gib']} GiB memory={env['memory_gib']} GiB")
    suite.log(f"  git={git_sha} dirty={env['git_dirty']}")
    suite.log(f"  gcc={env['gcc_version']}")
    suite.log(f"  clang={env['clang_version']}")

    if env["machine"] != "riscv64":
        suite.note("Host architecture is not riscv64; native performance steps are expected to fail preflight.")
    if qemu_like:
        suite.note("QEMU/TCG-like execution was detected; performance results must not be used.")
    if env["git_dirty"] is True:
        suite.note("Worktree is dirty. The suite records this state; use a clean commit for publishable comparisons.")
    elif env["git_dirty"] is None:
        suite.note("Git status is unavailable; commit provenance could not be verified.")
    if env["disk_free_gib"] < 8:
        suite.note("Less than 8 GiB free disk is available; full multi-compiler builds may run out of space.")
    if env["memory_gib"] is not None and env["memory_gib"] < 4:
        suite.note("Less than 4 GiB RAM is available; reduce --jobs if builds are unstable.")
    gcc_major = parse_major(env["gcc_version"])
    clang_major = parse_major(env["clang_version"])
    if gcc_major is not None and gcc_major < 14:
        suite.note(f"GCC {gcc_major} detected; GCC 14+ is the validated/recommended RVV toolchain.")
    if clang_major is not None and clang_major < 19:
        suite.note(f"Clang {clang_major} detected; Clang 19+ is the validated/recommended RVV toolchain.")
    if not which("taskset"):
        suite.note("taskset is unavailable; benchmarks cannot pin execution to one CPU and may be noisier.")


def compiler_available(name: str) -> bool:
    try:
        cc, cxx = compiler_commands(name)
    except ValueError:
        return False
    return bool(which(cc) and which(cxx))


def preflight_compiler(suite: Suite, compiler: str, core: int | None) -> bool:
    if not compiler_available(compiler):
        suite.skip(f"preflight-{compiler}", "preflight", f"{compiler} compiler pair is not installed")
        suite.preflights[compiler] = {"status": "SKIP"}
        return False
    workspace = suite.out_dir / "preflight" / compiler
    cmd = [sys.executable, str(PERF_SCRIPT), "preflight", "--workspace", str(workspace), "--compiler", compiler]
    if core is not None:
        cmd += ["--core", str(core)]
    step = suite.run_step(f"preflight-{compiler}", "preflight", cmd)
    meta_path = workspace / "metadata.json"
    meta: dict[str, Any] = {"status": step["status"]}
    if meta_path.exists():
        try:
            meta.update(json.loads(meta_path.read_text(encoding="utf-8")))
        except Exception as exc:
            meta["metadata_error"] = str(exc)
    suite.preflights[compiler] = meta
    return step["status"] == "PASS"


def correctness_step(suite: Suite, compiler: str, profile: str, jobs: int) -> None:
    if not compiler_available(compiler):
        suite.skip(f"correctness-{compiler}-{profile}", "correctness", f"{compiler} is not installed")
        return
    build_root = suite.out_dir / "builds" / "native-tests"
    cmd = [
        sys.executable, str(TEST_SCRIPT),
        "--compiler", compiler,
        "--profile", profile,
        "--suite", "rvv",
        "--build-root", str(build_root),
        "--jobs", str(jobs),
    ]
    suite.run_step(f"correctness-{compiler}-{profile}", "correctness", cmd)


def acceptance_step(suite: Suite, jobs: int) -> None:
    if not compiler_available("gcc"):
        suite.skip("acceptance-gcc-global", "acceptance", "gcc/g++ are not installed")
        return
    build_root = suite.out_dir / "builds" / "native-tests"
    cmd = [
        sys.executable, str(TEST_SCRIPT),
        "--compiler", "gcc",
        "--profile", "global",
        "--suite", "acceptance",
        "--build-root", str(build_root),
        "--jobs", str(jobs),
    ]
    suite.run_step("acceptance-gcc-global", "acceptance", cmd)


def geomean(values: list[float]) -> float | None:
    vals = [x for x in values if x > 0 and math.isfinite(x)]
    if not vals:
        return None
    return math.exp(sum(math.log(x) for x in vals) / len(vals))


def summarize_benchmark(workspace: Path) -> dict[str, Any]:
    summary = workspace / "results" / "summary.csv"
    raw = workspace / "results" / "raw.csv"
    report = workspace / "results" / "report.md"
    result: dict[str, Any] = {
        "report": str(report),
        "raw_csv": str(raw),
        "summary_csv": str(summary),
        "noisy_rows": 0,
        "current_vs_best_vls_geomean": {},
        "phase1_vs_legacy_geomean": {},
    }
    if not summary.exists():
        return result
    rows = list(csv.DictReader(summary.open(newline="", encoding="utf-8")))
    result["noisy_rows"] = sum(float(r.get("mad_percent", "nan")) > 3.0 for r in rows)
    idx = {(r["candidate"], r["corpus"], r["metric"]): r for r in rows}
    metrics = sorted({r["metric"] for r in rows})
    corpora = sorted({r["corpus"] for r in rows})
    upstream = sorted({r["candidate"] for r in rows if r.get("family") == "upstream"})
    for metric in metrics:
        ratios: list[float] = []
        for corpus in corpora:
            cur = idx.get(("current_vla", corpus, metric))
            ups = [idx.get((u, corpus, metric)) for u in upstream]
            ups = [u for u in ups if u]
            if not cur or not ups:
                continue
            best = max(ups, key=lambda r: float(r["median_gbps"]))
            denom = float(best["median_gbps"])
            if denom > 0:
                ratios.append(float(cur["median_gbps"]) / denom)
        gm = geomean(ratios)
        if gm is not None:
            result["current_vs_best_vls_geomean"][metric] = gm

    candidates = sorted({r["candidate"] for r in rows if r.get("family") == "experiment"} | {"current_vla"})
    if any(r["candidate"] == "vla_legacy" for r in rows):
        for metric in metrics:
            metric_map: dict[str, float] = {}
            for candidate in candidates:
                ratios = []
                for corpus in corpora:
                    base = idx.get(("vla_legacy", corpus, metric))
                    row = idx.get((candidate, corpus, metric))
                    if not base or not row:
                        continue
                    denom = float(base["median_gbps"])
                    if denom > 0:
                        ratios.append(float(row["median_gbps"]) / denom)
                gm = geomean(ratios)
                if gm is not None:
                    metric_map[candidate] = gm
            if metric_map:
                result["phase1_vs_legacy_geomean"][metric] = metric_map
    return result


def benchmark_step(suite: Suite, compiler: str, profile: str, variants: str,
                   core: int | None, vls_bits: str | None) -> None:
    if suite.preflights.get(compiler, {}).get("status") != "PASS":
        suite.skip(f"benchmark-{compiler}-{profile}", "benchmark", f"{compiler} native RVV preflight did not pass", required=False)
        return
    workspace = suite.out_dir / "benchmarks" / compiler
    cmd = [
        sys.executable, str(PERF_SCRIPT), "all",
        "--workspace", str(workspace),
        "--compiler", compiler,
        "--profile", profile,
        "--vla-variants", variants,
    ]
    if core is not None:
        cmd += ["--core", str(core)]
    if vls_bits:
        cmd += ["--vls-bits", vls_bits]
    step = suite.run_step(f"benchmark-{compiler}-{profile}", "benchmark", cmd)
    if step["status"] == "PASS":
        suite.benchmarks[compiler] = summarize_benchmark(workspace)


def network_probe(suite: Suite) -> None:
    if not which("git"):
        suite.skip("network-upstream", "preflight", "git is not installed", required=False)
        return
    cmd = ["git", "ls-remote", "--exit-code", "https://github.com/simdjson/simdjson.git", "HEAD"]
    suite.run_step("network-upstream", "preflight", cmd, required=False)


def required_tools_probe(suite: Suite) -> None:
    required = ["git", "cmake", "ctest", "python3"]
    missing = [name for name in required if not which(name)]
    if missing:
        suite.log("FAIL required-tools: missing " + ", ".join(missing))
        suite.add_step(name="required-tools", category="preflight", status="FAIL", required=True,
                       detail="missing: " + ", ".join(missing))
    else:
        suite.log("PASS required-tools")
        suite.add_step(name="required-tools", category="preflight", status="PASS", required=True)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="One-command native RVV VPS validation + correctness + performance matrix."
    )
    p.add_argument("--preset", choices=tuple(PRESETS), default="phase1",
                   help="smoke, phase1 (recommended first run), matrix, or standard")
    p.add_argument("--output", type=Path,
                   help="result directory; default ../rvv-native-runs/<UTC stamp>")
    p.add_argument("--jobs", type=int, default=max(1, min(8, os.cpu_count() or 1)))
    p.add_argument("--core", type=int, help="CPU to pin performance tests to")
    p.add_argument("--vls-bits", help="optional upstream VLS subset, e.g. 128,256")
    p.add_argument("--bench-profile", choices=("quick", "standard", "publication"),
                   help="override the preset benchmark profile")
    p.add_argument("--vla-variants",
                   help="override preset variants, e.g. current, phase1, all")
    p.add_argument("--skip-correctness", action="store_true")
    p.add_argument("--skip-benchmarks", action="store_true")
    p.add_argument("--acceptance", action="store_true",
                   help="force native GCC acceptance even when preset omits it")
    p.add_argument("--no-acceptance", action="store_true",
                   help="skip native acceptance even when preset includes it")
    p.add_argument("--quiet", action="store_true", help="write detailed output to logs without teeing child output")
    return p


def main() -> int:
    args = parser().parse_args()
    preset = PRESETS[args.preset]
    out_dir = (args.output or (REPO_ROOT.parent / "rvv-native-runs" / utc_stamp())).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    suite = Suite(out_dir, verbose=not args.quiet)

    suite.log("RVV native VPS validation suite")
    suite.log(f"preset={args.preset} output={out_dir}")
    snapshot_environment(suite)
    required_tools_probe(suite)
    network_probe(suite)

    preflight_targets = sorted(set(c for c, _ in preset["correctness"]) | set(preset["bench_compilers"]))
    for compiler in preflight_targets:
        preflight_compiler(suite, compiler, args.core)

    if not args.skip_correctness:
        for compiler, profile in preset["correctness"]:
            correctness_step(suite, compiler, profile, args.jobs)
        do_acceptance = (preset["acceptance"] or args.acceptance) and not args.no_acceptance
        if do_acceptance:
            acceptance_step(suite, args.jobs)
    else:
        suite.note("Correctness phase skipped by command-line request.")

    if not args.skip_benchmarks:
        bench_profile = args.bench_profile or str(preset["bench_profile"])
        variants = args.vla_variants or str(preset["vla_variants"])
        for compiler in preset["bench_compilers"]:
            benchmark_step(suite, compiler, bench_profile, variants, args.core, args.vls_bits)
    else:
        suite.note("Benchmark phase skipped by command-line request.")

    suite.save()
    summary = json.loads((out_dir / "summary.json").read_text(encoding="utf-8"))
    suite.log("")
    suite.log(f"FINAL: {summary['overall_status']}")
    suite.log(f"Summary: {out_dir / 'summary.md'}")
    suite.log(f"Bundle:  {out_dir / 'rvv-native-results.tar.gz'}")
    return 0 if summary["overall_status"] == "PASS" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        raise SystemExit(130)

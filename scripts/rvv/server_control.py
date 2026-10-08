#!/usr/bin/env python3
"""Interactive/CLI control panel for native simdjson RVV server validation.

This file deliberately contains orchestration only. Correctness is delegated to
scripts/rvv/native_suite.py + run_tests.py, and performance is delegated to
scripts/rvv/perf/bench_native.py through native_suite.py.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tarfile
import time
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
NATIVE_SUITE = SCRIPT_DIR / "native_suite.py"
PERF_SCRIPT = SCRIPT_DIR / "perf" / "bench_native.py"
DEFAULT_RESULTS_ROOT = REPO_ROOT.parent / "rvv-server-runs"

ACTIONS = (
    "doctor",
    "setup",
    "sync",
    "preflight",
    "smoke",
    "phase1",
    "matrix",
    "standard",
    "publication",
    "auto",
    "latest",
)


def now_stamp() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def iso_utc() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def qcmd(cmd: list[str]) -> str:
    return " ".join(shlex.quote(str(x)) for x in cmd)


def capture(cmd: list[str], cwd: Path | None = None) -> tuple[int, str]:
    try:
        p = subprocess.run(
            [str(x) for x in cmd],
            cwd=str(cwd) if cwd else None,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        return p.returncode, p.stdout.strip()
    except Exception as exc:
        return 127, f"<unavailable: {exc}>"


def first_line(cmd: list[str]) -> str:
    rc, out = capture(cmd)
    if rc != 0 or not out:
        return "unavailable"
    return out.splitlines()[0]


def which(name: str) -> str | None:
    return shutil.which(name)


def detect_virtualization() -> str:
    if which("systemd-detect-virt"):
        rc, out = capture(["systemd-detect-virt"])
        if out:
            return out.splitlines()[-1]
        if rc == 0:
            return "none"
    return "unknown"


def detect_qemu_like() -> bool:
    chunks: list[str] = []
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


def read_mem_gib() -> float | None:
    try:
        for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
            if line.startswith("MemTotal:"):
                return int(line.split()[1]) / (1024 ** 2)
    except (OSError, ValueError, IndexError):
        pass
    return None


def git_state() -> dict[str, Any]:
    rc_sha, sha = capture(["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD"])
    rc_branch, branch = capture(["git", "-C", str(REPO_ROOT), "branch", "--show-current"])
    rc_status, status = capture(["git", "-C", str(REPO_ROOT), "status", "--porcelain=v1"])
    rc_up, upstream = capture([
        "git", "-C", str(REPO_ROOT), "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"
    ])
    return {
        "sha": sha if rc_sha == 0 else "unavailable",
        "branch": branch if rc_branch == 0 else "unavailable",
        "dirty": bool(status) if rc_status == 0 else None,
        "dirty_text": status if rc_status == 0 else "unavailable",
        "upstream": upstream if rc_up == 0 else "unavailable",
    }


def machine_snapshot() -> dict[str, Any]:
    cpus = allowed_cpus()
    disk = shutil.disk_usage(REPO_ROOT)
    return {
        "timestamp_utc": iso_utc(),
        "machine": platform.machine(),
        "kernel": first_line(["uname", "-a"]),
        "virtualization": detect_virtualization(),
        "qemu_like": detect_qemu_like(),
        "cpus": cpus,
        "cpu_count": os.cpu_count(),
        "memory_gib": read_mem_gib(),
        "disk_free_gib": disk.free / (1024 ** 3),
        "python": sys.version.splitlines()[0],
        "git": git_state(),
        "cmake": first_line(["cmake", "--version"]) if which("cmake") else "unavailable",
        "ninja": first_line(["ninja", "--version"]) if which("ninja") else "unavailable",
        "gcc": first_line([os.environ.get("RVV_NATIVE_CXX", "g++"), "--version"])
        if which(os.environ.get("RVV_NATIVE_CXX", "g++")) else "unavailable",
        "clang": first_line([os.environ.get("RVV_NATIVE_CLANG_CXX", "clang++"), "--version"])
        if which(os.environ.get("RVV_NATIVE_CLANG_CXX", "clang++")) else "unavailable",
        "taskset": which("taskset") or "unavailable",
        "perf": which("perf") or "unavailable",
    }


def status_label(value: bool) -> str:
    return "OK" if value else "MISSING"


def doctor_report(snapshot: dict[str, Any]) -> tuple[str, bool]:
    git = snapshot["git"]
    required = {
        "git": bool(which("git")),
        "cmake": bool(which("cmake")),
        "ctest": bool(which("ctest")),
        "python3": bool(which("python3")),
        "C++ compiler": bool(which(os.environ.get("RVV_NATIVE_CXX", "g++"))),
    }
    lines = [
        "simdjson RVV server doctor",
        "=" * 66,
        f"UTC            : {snapshot['timestamp_utc']}",
        f"Architecture   : {snapshot['machine']}",
        f"Virtualization : {snapshot['virtualization']}",
        f"QEMU/TCG-like  : {snapshot['qemu_like']}",
        f"Allowed CPUs   : {snapshot['cpus']}",
        f"RAM            : {snapshot['memory_gib']:.2f} GiB" if snapshot['memory_gib'] is not None else "RAM            : unknown",
        f"Disk free      : {snapshot['disk_free_gib']:.2f} GiB",
        f"Git SHA        : {git['sha']}",
        f"Git branch     : {git['branch']}",
        f"Git upstream   : {git['upstream']}",
        f"Git dirty      : {git['dirty']}",
        f"GCC            : {snapshot['gcc']}",
        f"Clang          : {snapshot['clang']}",
        f"CMake          : {snapshot['cmake']}",
        f"Ninja          : {snapshot['ninja']}",
        f"taskset        : {snapshot['taskset']}",
        f"perf           : {snapshot['perf']}",
        "",
        "Required tools:",
    ]
    for name, present in required.items():
        lines.append(f"  {status_label(present):7s} {name}")

    blocking = []
    warnings = []
    if snapshot["machine"] != "riscv64":
        blocking.append("architecture is not riscv64")
    if snapshot["qemu_like"]:
        blocking.append("QEMU/TCG-like execution detected")
    if not all(required.values()):
        blocking.append("one or more required build tools are missing")
    if git["dirty"] is True:
        warnings.append("worktree is dirty; benchmark provenance will not be publishable")
    if snapshot["disk_free_gib"] < 8:
        warnings.append("less than 8 GiB free disk")
    if snapshot["memory_gib"] is not None and snapshot["memory_gib"] < 4:
        warnings.append("less than 4 GiB RAM; use fewer build jobs if needed")
    if not which("taskset"):
        warnings.append("taskset is missing; benchmark CPU pinning will be unavailable")
    if snapshot["virtualization"] not in ("none", "unknown"):
        warnings.append("virtualized host: results are VPS-relative and may be noisy")

    lines += ["", "Assessment:"]
    if blocking:
        for item in blocking:
            lines.append(f"  BLOCK: {item}")
    else:
        lines.append("  READY: basic native-performance prerequisites look good")
    for item in warnings:
        lines.append(f"  WARN : {item}")
    lines += [
        "",
        "Note: the authoritative RVV/VLEN check is the compiled preflight probe.",
    ]
    return "\n".join(lines) + "\n", not blocking


class Controller:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.results_root = args.results_root.expanduser().resolve()
        self.results_root.mkdir(parents=True, exist_ok=True)
        self.session = self.results_root / f"control-{now_stamp()}"
        self.session.mkdir(parents=True, exist_ok=True)
        self.log_path = self.session / "control.log"
        self.actions: list[dict[str, Any]] = []

    def log(self, msg: str = "") -> None:
        line = f"[{dt.datetime.now().strftime('%H:%M:%S')}] {msg}" if msg else ""
        print(line, flush=True)
        with self.log_path.open("a", encoding="utf-8") as f:
            f.write(line + "\n")

    def record(self, name: str, rc: int, duration: float, command: list[str] | None = None,
               output: str | None = None) -> None:
        self.actions.append({
            "name": name,
            "returncode": rc,
            "status": "PASS" if rc == 0 else "FAIL",
            "duration_seconds": round(duration, 3),
            "command": qcmd(command) if command else "",
            "output": output or "",
        })
        self.write_summary()
        self.make_bundle()

    def write_summary(self) -> None:
        data = {
            "generated_utc": iso_utc(),
            "repo_root": str(REPO_ROOT),
            "session": str(self.session),
            "actions": self.actions,
        }
        (self.session / "control-summary.json").write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        lines = ["# RVV server control session", "", f"- UTC: `{data['generated_utc']}`", "", "| Action | Status | Duration |", "|---|---|---:|"]
        for row in self.actions:
            lines.append(f"| {row['name']} | **{row['status']}** | {row['duration_seconds']:.1f}s |")
        lines.append("")
        (self.session / "control-summary.md").write_text("\n".join(lines), encoding="utf-8")

    def make_bundle(self) -> None:
        bundle = self.session / "rvv-server-control-results.tar.gz"
        with tarfile.open(bundle, "w:gz") as tf:
            for name in ("control-summary.json", "control-summary.md", "control.log", "doctor.json", "doctor.txt"):
                p = self.session / name
                if p.exists():
                    tf.add(p, arcname=name)
            for p in sorted(self.session.glob("*.log")):
                if p.name != "control.log":
                    tf.add(p, arcname=p.name)
            for p in sorted(self.session.rglob("rvv-native-results.tar.gz")):
                tf.add(p, arcname=str(p.relative_to(self.session)))
            for p in sorted(self.session.glob("preflight-*/metadata.json")):
                tf.add(p, arcname=str(p.relative_to(self.session)))

    def run(self, name: str, command: list[str], *, cwd: Path = REPO_ROOT) -> int:
        self.log("")
        self.log(f"=== {name} ===")
        self.log("> " + qcmd(command))
        step_log = self.session / (name.replace("/", "_").replace(" ", "_") + ".log")
        start = time.monotonic()
        rc = 127
        with step_log.open("w", encoding="utf-8") as lf:
            lf.write("> " + qcmd(command) + "\n\n")
            try:
                proc = subprocess.Popen(
                    [str(x) for x in command],
                    cwd=str(cwd),
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    bufsize=1,
                )
                assert proc.stdout is not None
                for line in proc.stdout:
                    print(line, end="", flush=True)
                    lf.write(line)
                    with self.log_path.open("a", encoding="utf-8") as cf:
                        cf.write(line)
                rc = proc.wait()
            except Exception as exc:
                text = f"CONTROL_EXEC_ERROR: {exc}\n"
                print(text, end="", file=sys.stderr)
                lf.write(text)
        duration = time.monotonic() - start
        self.log(f"{'PASS' if rc == 0 else 'FAIL'} {name} ({duration:.1f}s, rc={rc})")
        self.record(name, rc, duration, command=command, output=str(step_log))
        return rc

    def suite_cmd(self, preset: str, output: Path, *, bench_profile: str | None = None,
                  skip_benchmarks: bool = False, skip_correctness: bool = False) -> list[str]:
        cmd = [sys.executable, str(NATIVE_SUITE), "--preset", preset, "--output", str(output), "--jobs", str(self.args.jobs)]
        if self.args.core is not None:
            cmd += ["--core", str(self.args.core)]
        if self.args.vls_bits:
            cmd += ["--vls-bits", self.args.vls_bits]
        if bench_profile:
            cmd += ["--bench-profile", bench_profile]
        if self.args.vla_variants:
            cmd += ["--vla-variants", self.args.vla_variants]
        if skip_benchmarks:
            cmd.append("--skip-benchmarks")
        if skip_correctness:
            cmd.append("--skip-correctness")
        return cmd

    def action_doctor(self) -> int:
        start = time.monotonic()
        snap = machine_snapshot()
        report, ready = doctor_report(snap)
        print(report, end="")
        (self.session / "doctor.json").write_text(json.dumps(snap, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (self.session / "doctor.txt").write_text(report, encoding="utf-8")
        rc = 0 if ready else 2
        self.record("doctor", rc, time.monotonic() - start, output=str(self.session / "doctor.txt"))
        return rc

    def action_setup(self) -> int:
        packages = ["git", "cmake", "ninja-build", "build-essential", "python3", "file", "util-linux"]
        if self.args.with_clang:
            packages.append("clang")
        if not Path("/etc/debian_version").exists() or not which("apt-get"):
            self.log("Automatic package setup is supported only on Debian/Ubuntu apt hosts.")
            self.record("setup", 2, 0.0, output="unsupported package manager")
            return 2
        if not self.args.yes and sys.stdin.isatty():
            print("Packages to install/update: " + " ".join(packages))
            answer = input("Continue with apt? [y/N] ").strip().lower()
            if answer not in {"y", "yes", "o", "oui"}:
                self.log("Setup cancelled.")
                self.record("setup", 0, 0.0, output="cancelled")
                return 0
        elif not self.args.yes:
            self.log("Refusing non-interactive apt install without --yes.")
            self.record("setup", 2, 0.0, output="use --yes")
            return 2
        prefix: list[str] = [] if os.geteuid() == 0 else (["sudo"] if which("sudo") else [])
        if os.geteuid() != 0 and not prefix:
            self.log("Need root or sudo to install packages.")
            self.record("setup", 2, 0.0, output="root/sudo required")
            return 2
        rc = self.run("setup-apt-update", [*prefix, "apt-get", "update"])
        if rc != 0:
            return rc
        return self.run("setup-apt-install", [*prefix, "apt-get", "install", "-y", *packages])

    def action_sync(self) -> int:
        state = git_state()
        if state["dirty"] is True:
            self.log("Refusing git pull: worktree is dirty.")
            self.record("sync", 2, 0.0, output=state["dirty_text"])
            return 2
        if state["upstream"] == "unavailable":
            self.log("Refusing git pull: no upstream branch is configured.")
            self.record("sync", 2, 0.0, output="no upstream")
            return 2
        return self.run("sync", ["git", "pull", "--ff-only"])

    def action_preflight(self) -> int:
        rc_total = 0
        compilers = ["gcc"]
        if which(os.environ.get("RVV_NATIVE_CLANG_CXX", "clang++")):
            compilers.append("clang")
        for compiler in compilers:
            out = self.session / f"preflight-{compiler}"
            cmd = [sys.executable, str(PERF_SCRIPT), "preflight", "--workspace", str(out), "--compiler", compiler]
            if self.args.core is not None:
                cmd += ["--core", str(self.args.core)]
            rc = self.run(f"preflight-{compiler}", cmd)
            if rc != 0:
                rc_total = rc
        return rc_total

    def action_suite(self, preset: str, *, profile: str | None = None) -> int:
        out = self.session / preset
        if profile:
            out = self.session / f"{preset}-{profile}"
        cmd = self.suite_cmd(preset, out, bench_profile=profile)
        return self.run(f"suite-{preset}{'-' + profile if profile else ''}", cmd)

    def action_auto(self) -> int:
        self.log("AUTO policy: doctor -> preflight -> smoke -> phase1; each performance stage is gated by the previous stage.")
        if self.action_doctor() != 0:
            self.log("AUTO STOP: doctor found a blocking prerequisite.")
            return 2
        if self.action_preflight() != 0:
            self.log("AUTO STOP: compiled native RVV/VLEN preflight failed.")
            return 2
        smoke_out = self.session / "auto-smoke"
        rc = self.run("auto-smoke", self.suite_cmd("smoke", smoke_out))
        if rc != 0:
            self.log("AUTO STOP: smoke suite failed; phase1 was not started.")
            return rc
        phase1_out = self.session / "auto-phase1"
        rc = self.run("auto-phase1", self.suite_cmd("phase1", phase1_out))
        if rc == 0:
            self.log("AUTO COMPLETE: smoke + phase1 passed. Use matrix/standard next if desired.")
        return rc

    def action_latest(self) -> int:
        start = time.monotonic()
        candidates = sorted(
            (p for p in self.results_root.rglob("summary.md") if p.is_file()),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )
        if not candidates:
            text = f"No native_suite summary.md found under {self.results_root}\n"
            print(text, end="")
            self.record("latest", 1, time.monotonic() - start, output=text.strip())
            return 1
        summary = candidates[0]
        text = summary.read_text(encoding="utf-8", errors="replace")
        print(f"Latest report: {summary}\n\n{text}", end="")
        self.record("latest", 0, time.monotonic() - start, output=str(summary))
        return 0


def print_banner() -> None:
    snap = machine_snapshot()
    git = snap["git"]
    print("\n" + "=" * 72)
    print("  simdjson RVV Server Control")
    print("=" * 72)
    print(f"  Host   : {snap['machine']}  virt={snap['virtualization']}  qemu={snap['qemu_like']}")
    print(f"  Git    : {str(git['sha'])[:12]}  branch={git['branch']}  dirty={git['dirty']}")
    print(f"  GCC    : {snap['gcc']}")
    print(f"  Clang  : {snap['clang']}")
    print(f"  CPUs   : {snap['cpus']}")
    print("=" * 72)


def interactive(controller: Controller) -> int:
    menu = [
        ("1", "Doctor / machine overview", "doctor"),
        ("2", "Install/check Debian prerequisites", "setup"),
        ("3", "Git sync (--ff-only, clean tree only)", "sync"),
        ("4", "Native RVV + VLEN preflight", "preflight"),
        ("5", "Smoke: GCC correctness + quick current VLA/VLS", "smoke"),
        ("6", "Phase1: recommended A/B campaign", "phase1"),
        ("7", "Matrix: GCC/Clang correctness + acceptance + quick A/B", "matrix"),
        ("8", "Standard: full matrix + standard benchmarks", "standard"),
        ("9", "Publication: full matrix + long publication benchmarks", "publication"),
        ("A", "AUTO SAFE: doctor -> preflight -> smoke -> phase1", "auto"),
        ("L", "Show latest native-suite report", "latest"),
        ("Q", "Quit", "quit"),
    ]
    while True:
        print_banner()
        for key, title, _ in menu:
            print(f"  {key:>2}. {title}")
        print("")
        choice = input("Choice: ").strip().upper()
        selected = next((action for key, _, action in menu if key == choice), None)
        if selected is None:
            print("Unknown choice.")
            continue
        if selected == "quit":
            return 0
        rc = dispatch(controller, selected)
        print(f"\nAction finished with rc={rc}. Session logs: {controller.session}")
        print(f"Shareable bundle: {controller.session / 'rvv-server-control-results.tar.gz'}")
        input("Press Enter to continue...")


def dispatch(controller: Controller, action: str) -> int:
    if action == "doctor":
        return controller.action_doctor()
    if action == "setup":
        return controller.action_setup()
    if action == "sync":
        return controller.action_sync()
    if action == "preflight":
        return controller.action_preflight()
    if action in {"smoke", "phase1", "matrix", "standard"}:
        return controller.action_suite(action)
    if action == "publication":
        return controller.action_suite("standard", profile="publication")
    if action == "auto":
        return controller.action_auto()
    if action == "latest":
        return controller.action_latest()
    raise ValueError(action)


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Interactive and scripted control panel for native RVV server validation."
    )
    p.add_argument("action", nargs="?", choices=ACTIONS,
                   help="action to run; omit for the interactive menu")
    p.add_argument("--auto", action="store_true", help="alias for the safe auto workflow")
    p.add_argument("--results-root", type=Path, default=DEFAULT_RESULTS_ROOT,
                   help="control sessions and native-suite results root")
    p.add_argument("--jobs", type=int, default=max(1, min(8, os.cpu_count() or 1)))
    p.add_argument("--core", type=int, help="CPU used by pinned performance runs")
    p.add_argument("--vls-bits", help="optional upstream VLS subset, e.g. 128,256")
    p.add_argument("--vla-variants", help="override native_suite benchmark variants")
    p.add_argument("--with-clang", action="store_true", help="include clang in apt setup")
    p.add_argument("-y", "--yes", action="store_true", help="allow non-interactive apt installation")
    return p


def main() -> int:
    args = parser().parse_args()
    if args.auto:
        if args.action and args.action != "auto":
            print("--auto cannot be combined with a different action", file=sys.stderr)
            return 2
        args.action = "auto"
    controller = Controller(args)
    controller.write_summary()
    if args.action:
        return dispatch(controller, args.action)
    if not sys.stdin.isatty():
        print("Interactive menu requires a TTY. Use an explicit action or --auto.", file=sys.stderr)
        return 2
    return interactive(controller)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        raise SystemExit(130)

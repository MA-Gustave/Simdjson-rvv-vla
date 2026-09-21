#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

PREREQ_MISSING = 77


def log_command(cmd):
    print("> " + " ".join(shlex.quote(str(x)) for x in cmd), flush=True)


def run(cmd, *, cwd, env=None):
    log_command(cmd)
    cp = subprocess.run(cmd, cwd=str(cwd), env=env, check=False)
    if cp.returncode != 0:
        raise SystemExit(cp.returncode)


def output(cmd, *, cwd=None):
    return subprocess.check_output(cmd, cwd=str(cwd) if cwd else None, text=True, stderr=subprocess.STDOUT).strip()


def parse_version(text):
    for token in text.replace("-", ".").split("."):
        if token.isdigit():
            return int(token)
    return 0


def repo_root():
    return Path(__file__).resolve().parents[2]


def reexec_under_wsl(args):
    if os.name != "nt" or args.inside_wsl:
        return None
    if shutil.which("wsl.exe") is None:
        print("RVV_TEST_PREREQ_MISSING: wsl.exe is not available", file=sys.stderr)
        return PREREQ_MISSING

    # Deterministic Windows -> WSL transition.
    # Raw C:\\... paths can lose their backslashes before wslpath sees them.
    # Forward-slash Windows paths are accepted by wslpath and avoid that issue.
    distro = args.wsl_distro
    root = repo_root()
    windows_root = str(root.resolve()).replace("\\", "/")

    try:
        linux_root = output([
            "wsl.exe", "-d", distro, "--",
            "wslpath", "-u", "-a", windows_root,
        ])
    except (subprocess.CalledProcessError, OSError) as exc:
        print(
            f"RVV_TEST_PREREQ_MISSING: cannot translate repository path "
            f"through WSL distro {distro!r}: {exc}",
            file=sys.stderr,
        )
        return PREREQ_MISSING

    forwarded = []
    for item in sys.argv[1:]:
        if item == "--inside-wsl":
            continue
        forwarded.append(item)
    forwarded.append("--inside-wsl")

    command = (
        "cd " + shlex.quote(linux_root) +
        " && python3 scripts/rvv/run_tests.py " +
        " ".join(shlex.quote(x) for x in forwarded)
    )
    cp = subprocess.run(
        ["wsl.exe", "-d", distro, "--", "bash", "-lc", command],
        check=False,
    )
    return cp.returncode


def find_sysroot(compiler):
    env_value = os.environ.get("RVV_SYSROOT")
    if env_value:
        return env_value
    conventional = Path("/usr/riscv64-linux-gnu")
    if conventional.exists():
        return str(conventional)
    try:
        value = output([compiler, "-print-sysroot"])
        if value and value != "/":
            return value
    except Exception:
        pass
    return ""


def require_tool(name):
    path = shutil.which(name)
    if not path:
        print(f"RVV_TEST_PREREQ_MISSING: {name} is not available", file=sys.stderr)
        raise SystemExit(PREREQ_MISSING)
    return path


def compiler_setup(compiler, profile, *, native=False):
    if compiler == "gcc":
        if native:
            cc = require_tool(os.environ.get("RVV_NATIVE_CC", "gcc"))
            cxx = require_tool(os.environ.get("RVV_NATIVE_CXX", "g++"))
        else:
            cc = require_tool("riscv64-linux-gnu-gcc")
            cxx = require_tool("riscv64-linux-gnu-g++")
        if profile == "dispatch":
            try:
                major = int(output([cxx, "-dumpfullversion", "-dumpversion"]).split(".")[0])
            except Exception:
                major = 0
            if major < 14:
                print("RVV_TEST_PREREQ_MISSING: GCC 14+ is required for the dispatch profile", file=sys.stderr)
                raise SystemExit(PREREQ_MISSING)
        toolchain = "cmake/toolchains/rvv-vla-qemu-gcc.cmake"
    else:
        if native:
            cc = require_tool(os.environ.get("RVV_NATIVE_CLANG_CC", "clang"))
            cxx = require_tool(os.environ.get("RVV_NATIVE_CLANG_CXX", "clang++"))
        else:
            cc = require_tool("clang")
            cxx = require_tool("clang++")
        toolchain = "cmake/toolchains/rvv-vla-qemu-clang.cmake"
    return cc, cxx, toolchain


def native_riscv():
    machine = platform.machine().lower()
    return machine in {"riscv64", "riscv64gc"} or machine.startswith("riscv64")


def main():
    p = argparse.ArgumentParser(description="Build and run simdjson RVV-VLA tests")
    p.add_argument("--compiler", choices=["gcc", "clang"], default="gcc")
    p.add_argument("--profile", choices=["global", "dispatch"], default="global")
    p.add_argument("--suite", choices=["rvv", "acceptance", "all"], default="rvv")
    p.add_argument("--vlens", default="128,256,512,1024")
    p.add_argument("--build-root", default=".repodiag/builds")
    p.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    p.add_argument(
        "--wsl-distro",
        default=os.environ.get("RVV_WSL_DISTRO", "Debian"),
        help=argparse.SUPPRESS,
    )
    p.add_argument("--inside-wsl", action="store_true", help=argparse.SUPPRESS)
    args = p.parse_args()

    wsl_rc = reexec_under_wsl(args)
    if wsl_rc is not None:
        return wsl_rc

    root = repo_root()
    require_tool("cmake")
    require_tool("ctest")

    is_native = native_riscv()
    if not is_native:
        require_tool("qemu-riscv64")

    cc, cxx, toolchain = compiler_setup(args.compiler, args.profile, native=is_native)
    sysroot = find_sysroot(cc)

    build = (root / args.build_root / f"{args.compiler}-{args.profile}").resolve()
    build.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["RVV_MARCH"] = "rv64gcv" if args.profile == "global" else "rv64gc"
    cpp_flags = [
        "-DSIMDJSON_IMPLEMENTATION_RVV=1",
        "-DSIMDJSON_IMPLEMENTATION_FALLBACK=1",
        "-DSIMDJSON_IMPLEMENTATION_RVV_VLS=0",
    ]
    env["RVV_CPP_FLAGS"] = " ".join(cpp_flags)
    if sysroot:
        env["RVV_SYSROOT"] = sysroot

    configure = [
        "cmake", "-S", str(root), "-B", str(build),
        "-DSIMDJSON_DEVELOPER_MODE=ON",
        "-DSIMDJSON_RVV_TESTS=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    if is_native:
        configure += [
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DCMAKE_C_FLAGS=-march={env['RVV_MARCH']} -mabi=lp64d",
            f"-DCMAKE_CXX_FLAGS=-march={env['RVV_MARCH']} -mabi=lp64d {env['RVV_CPP_FLAGS']}",
        ]
    else:
        configure.append(f"-DCMAKE_TOOLCHAIN_FILE={root / toolchain}")

    run(configure, cwd=root, env=env)

    targets = []
    if args.suite in {"rvv", "all"}:
        targets.append("rvv_tests")
    if args.suite in {"acceptance", "all"}:
        targets.append("acceptance_tests")
    for target in targets:
        run(["cmake", "--build", str(build), "--target", target, "-j", str(args.jobs)], cwd=root, env=env)

    def run_ctest(label, vlen=None):
        test_env = env.copy()
        test_env["SIMDJSON_FORCE_IMPLEMENTATION"] = "rvv"
        if vlen is not None:
            test_env["QEMU_CPU"] = f"rv64,v=true,vext_spec=v1.0,vlen={vlen}"
        run([
            "ctest", "--test-dir", str(build), "-L", label,
            "--output-on-failure", "-j", str(args.jobs)
        ], cwd=root, env=test_env)

    labels = []
    if args.suite in {"rvv", "all"}:
        labels.append("^rvv$")
    if args.suite in {"acceptance", "all"}:
        labels.append("^acceptance$")

    if is_native:
        for label in labels:
            run_ctest(label)
    else:
        vlens = [int(x.strip()) for x in args.vlens.split(",") if x.strip()]
        for vlen in vlens:
            if vlen < 128 or vlen % 64 != 0:
                print(f"invalid VLEN requested: {vlen}", file=sys.stderr)
                return 2
            print(f"=== QEMU RVV VLEN={vlen} ===", flush=True)
            for label in labels:
                run_ctest(label, vlen)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

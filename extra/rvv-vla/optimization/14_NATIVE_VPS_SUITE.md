# Native VPS Validation and Benchmark Suite

## Purpose

`scripts/rvv/native_suite.py` is the one-command entry point for the first real
RVV host. It wraps the existing correctness runner and native performance lab so
a VPS can be qualified, tested and benchmarked without manually stitching
commands together.

The suite is intentionally conservative:

- performance requires native `riscv64` and the existing RVV VLEN probe;
- QEMU/TCG-like hosts are reported and rejected by `bench_native.py`;
- correctness and benchmark steps get separate logs;
- failures do not stop later independent diagnostic steps;
- the process exits non-zero when a required step failed;
- raw benchmark CSV and metadata are preserved.

## Native compiler fix

The regular correctness runner supports both cross/QEMU and native execution.
On a native RISC-V host GCC now resolves to normal `gcc` / `g++` rather than
requiring the cross-tool names `riscv64-linux-gnu-gcc` /
`riscv64-linux-gnu-g++`.

The native names can be overridden when required:

```bash
RVV_NATIVE_CC=gcc-14 RVV_NATIVE_CXX=g++-14 \
  python3 scripts/rvv/native_suite.py --preset phase1
```

Clang can be overridden independently with `RVV_NATIVE_CLANG_CC` and
`RVV_NATIVE_CLANG_CXX`. The same variables are honored by the correctness
runner and the native performance harness. Cross/QEMU behavior is unchanged.

## Recommended first VPS run

From the repository root:

```bash
python3 scripts/rvv/native_suite.py --preset phase1
```

`phase1` performs:

1. machine, Git, CPU, memory, disk and toolchain snapshot;
2. connectivity probe for the pristine upstream simdjson reference;
3. native RVV preflight/VLEN probe for GCC and, when installed, Clang;
4. GCC global correctness;
5. GCC runtime-dispatch correctness;
6. Clang global correctness when Clang is installed;
7. GCC quick native benchmark with all phase-1 A/B variants.

On VLEN=256 the benchmark automatically considers upstream fixed-width
RVV-VLS 128 and 256 builds. It also measures current VLA, fallback and the
isolated phase-1 VLA candidates.

## Presets

### `smoke`

Minimal bring-up:

```bash
python3 scripts/rvv/native_suite.py --preset smoke
```

Runs GCC global RVV correctness and a quick current-VLA versus upstream-VLS
benchmark.

### `phase1`

Recommended first performance run:

```bash
python3 scripts/rvv/native_suite.py --preset phase1
```

Adds GCC dispatch, optional Clang global correctness, and the complete phase-1
A/B benchmark under GCC.

### `matrix`

Broader compiler/runtime matrix:

```bash
python3 scripts/rvv/native_suite.py --preset matrix
```

Runs GCC and Clang global/dispatch correctness where the compiler exists,
GCC native acceptance, then quick phase-1 benchmarks for GCC and Clang.

### `standard`

Same coverage as `matrix`, but the performance phase uses the existing
`standard` benchmark profile:

```bash
python3 scripts/rvv/native_suite.py --preset standard
```

Use this only after the quick matrix is stable.

## Useful overrides

Pin performance measurements to a particular CPU:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --core 2
```

Restrict upstream fixed-width VLS builds:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --vls-bits 128,256
```

Force acceptance in the phase-1 preset:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --acceptance
```

Collect correctness only:

```bash
python3 scripts/rvv/native_suite.py --preset matrix --skip-benchmarks
```

Collect performance only after correctness has already been established:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --skip-correctness
```

Override the performance profile or VLA candidates:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 \
  --bench-profile standard --vla-variants legacy,packed-index,packed-quote
```

## Output

Every invocation creates a fresh timestamped directory by default:

```text
../rvv-native-runs/20260921T223000Z/
  summary.md
  summary.json
  suite.log
  rvv-native-results.tar.gz
  environment/
    environment.json
    uname.txt
    lscpu.txt
    cpuinfo.txt
    meminfo.txt
    os-release.txt
    df.txt
    git-status.txt
    git-log.txt
  steps/
    01_....log
    02_....log
  preflight/
    gcc/
    clang/
  builds/
    native-tests/
  benchmarks/
    gcc/
      metadata.json
      build_plan.json
      results/raw.csv
      results/summary.csv
      results/report.md
    clang/
      ...
```

The compressed `rvv-native-results.tar.gz` deliberately excludes bulky build
trees and upstream source clones. It contains the information needed to review
the run: summaries, environment data, step logs, benchmark metadata and raw
results.

## Result policy

A run is performance-usable only when the corresponding compiler's native RVV
preflight passes. `summary.md` records the detected VLEN, virtualization state,
compiler, CPU affinity and benchmark noise.

A VPS is useful for controlled A/B comparisons, but a virtualized result should
still be described as VPS-relative. QEMU/TCG results remain correctness-only.

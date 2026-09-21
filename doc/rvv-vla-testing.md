# RVV-VLA testing and diagnostics

## Validation model

The RVV-VLA backend uses two distinct validation environments:

1. **QEMU + cross toolchains** for deterministic correctness coverage across
   multiple simulated VLEN values.
2. **Native RISC-V Vector 1.0 hardware** for final correctness confirmation and
   all performance measurements.

QEMU is never a performance oracle. Throughput, cycle and optimization decisions
must come from a native RVV host.

## Dedicated RVV tests

The complete upstream simdjson test suite remains in the repository.

The dedicated RVV suite is in `tests/rvv/` and is enabled with:

```text
-DSIMDJSON_RVV_TESTS=ON
```

Dedicated executables:

- `rvv_registry_tests`
- `rvv_dom_equivalence_tests`
- `rvv_stage1_boundary_tests`
- `rvv_minify_tests`
- `rvv_utf8_tests`
- `rvv_number_tests`
- `rvv_ondemand_tests`
- `rvv_streaming_tests`
- `rvv_random_differential_tests`
- `rvv_corpus_tests`

Where practical, tests compare the `rvv` backend against fallback/reference
behavior.

## QEMU correctness matrix

The authoritative correctness runner is:

```text
scripts/rvv/run_tests.py
```

Validated QEMU VLEN values are 128, 256, 512 and 1024 bits.

Examples:

```bash
python3 scripts/rvv/run_tests.py --compiler gcc --profile global --suite rvv --vlens 128,256,512,1024 --jobs 8
python3 scripts/rvv/run_tests.py --compiler gcc --profile dispatch --suite rvv --vlens 128,256,512,1024 --jobs 8
python3 scripts/rvv/run_tests.py --compiler clang --profile global --suite rvv --vlens 128,256,512,1024 --jobs 8
python3 scripts/rvv/run_tests.py --compiler gcc --profile global --suite acceptance --vlens 128,256,512,1024 --jobs 8
```

Toolchain files:

- `cmake/toolchains/rvv-vla-qemu-gcc.cmake`
- `cmake/toolchains/rvv-vla-qemu-clang.cmake`

On Windows, the runner can re-enter the configured Debian WSL environment.
On a non-native host it requires `qemu-riscv64` and the cross toolchains.

## Native correctness runner behavior

The same `scripts/rvv/run_tests.py` is also used on native `riscv64` hosts.
When the host is native RISC-V, it does not require the cross compiler names.
It uses the normal host compilers instead:

- GCC: `gcc` / `g++`
- Clang: `clang` / `clang++`

The native compiler names can be overridden when a VPS exposes versioned
binaries:

```bash
RVV_NATIVE_CC=gcc-14 RVV_NATIVE_CXX=g++-14 \
  python3 scripts/rvv/run_tests.py --compiler gcc --profile global --suite rvv
```

Clang overrides are:

```text
RVV_NATIVE_CLANG_CC
RVV_NATIVE_CLANG_CXX
```

These variables are shared with the native performance harness.

## One-command native VPS suite

The recommended entry point for a new real RVV machine is:

```text
scripts/rvv/native_suite.py
```

It orchestrates environment qualification, native VLEN probing, correctness,
acceptance when requested, native performance measurements and result logging.
It deliberately reuses `run_tests.py` and `perf/bench_native.py` so there is one
source of truth for the underlying tests and benchmarks.

### First bring-up

Start with the small smoke preset:

```bash
python3 scripts/rvv/native_suite.py --preset smoke
```

If that passes, run the recommended phase-1 campaign:

```bash
python3 scripts/rvv/native_suite.py --preset phase1
```

### Presets

| Preset | Native correctness | Acceptance | Benchmarks |
|---|---|---|---|
| `smoke` | GCC global | no | GCC quick, current VLA |
| `phase1` | GCC global + dispatch; Clang global when installed | no | GCC quick, full phase-1 A/B variants |
| `matrix` | GCC + Clang global/dispatch when available | GCC native acceptance | GCC + Clang quick, phase-1 A/B |
| `standard` | same as `matrix` | GCC native acceptance | GCC + Clang standard, phase-1 A/B |

Use `standard` only after the quick matrix is stable.

### Useful overrides

Pin benchmarks to a CPU:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --core 2
```

Restrict upstream VLS widths:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --vls-bits 128,256
```

Force acceptance in `phase1`:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --acceptance
```

Correctness only:

```bash
python3 scripts/rvv/native_suite.py --preset matrix --skip-benchmarks
```

Performance only after correctness has already been established:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 --skip-correctness
```

Override the benchmark profile or VLA candidates:

```bash
python3 scripts/rvv/native_suite.py --preset phase1 \
  --bench-profile standard --vla-variants legacy,packed-index,packed-quote
```

## Native preflight requirements

The performance harness requires a real native RISC-V execution environment.
Its preflight checks include:

- `uname -m` must report `riscv64`;
- the selected compiler must build and execute the RVV VLEN probe;
- the detected VLEN must be plausible;
- QEMU/TCG-like execution is rejected for performance by default;
- the selected CPU affinity must be valid;
- host/compiler/Git metadata is captured for reproducibility.

The first intended hardware target is SpacemiT K1/X60 with VLEN=256, such as a
Banana Pi BPI-F3 or a native VPS/server exposing the same RVV hardware.

A virtualized host with hardware-backed RISC-V execution can be useful for
comparative A/B work, but its results should be described as VPS-relative.

## Native performance harness

The lower-level performance harness is:

```text
scripts/rvv/perf/bench_native.py
```

It compares:

- current scalable `rvv` VLA;
- current fallback;
- pristine upstream simdjson v4.6.11 / `f5de14f` `rvv_vls` builds at usable
  fixed widths;
- upstream fallback;
- optional isolated VLA optimization variants.

Measured metrics are:

- `dom_all`
- `stage1`
- `stage2`
- `minify`
- `utf8`

The recommended first A/B variant set is `phase1`, which contains:

- `vla_legacy`
- `vla_sparse_index`
- `vla_packed_index`
- `vla_sparse_escape`
- `vla_packed_escape`
- `vla_packed_quote`
- `vla_packed_shift`
- `current_vla`

Direct lower-level invocation remains available:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile quick --compiler gcc --vla-variants phase1
```

For the first VPS run, prefer `native_suite.py` because it also records the
qualification and correctness context.

## Native result layout

Every `native_suite.py` invocation creates a new timestamped directory by
default under:

```text
../rvv-native-runs/<UTC stamp>/
```

Important outputs include:

```text
summary.md
summary.json
suite.log
rvv-native-results.tar.gz
environment/
steps/
preflight/
builds/
benchmarks/gcc/results/raw.csv
benchmarks/gcc/results/summary.csv
benchmarks/gcc/results/report.md
```

Clang benchmark results are stored under `benchmarks/clang/` when that preset
runs them.

The compact `rvv-native-results.tar.gz` intentionally excludes bulky build trees
and upstream clones. It is the preferred artifact to archive or share for
analysis because it contains the summaries, environment metadata, step logs and
raw benchmark results.

## Benchmark interpretation

Use results only when the matching native preflight passes.

For performance review:

- compare candidates from the same machine and run;
- keep raw CSV and metadata with any claim;
- use median throughput rather than one sample;
- inspect MAD/median noise, with values above roughly 3% treated cautiously;
- repeat noisy results with more trials or a quieter/pinned CPU;
- do not mix compiler families in one speedup claim;
- do not convert QEMU timing into hardware performance evidence.

## External RepoDiag orchestration

The optional generic diagnostics/orchestration tool is maintained in the
separate **RepoDiag** repository. Point it at this repository explicitly, for
example:

```text
python ..\RepoDiagSimdjson\repodiag.py --target C:\mycode\Simdjson\simdjson-rvv-vla run rvv-full --jobs 8
```

RepoDiag delegates RVV cross-build and QEMU execution to
`scripts/rvv/run_tests.py`; the authoritative tests and runner remain in this
repository. This keeps upstream-facing simdjson changes independent of the
diagnostics framework.

## Related design and experiment documents

See:

- `extra/rvv-vla/spec/09_PERFORMANCE_BENCHMARKING.md`
- `extra/rvv-vla/spec/23_BENCHMARK_EXPERIMENT_MATRIX.md`
- `extra/rvv-vla/optimization/13_PACKED_INDEX_AND_AB_HARNESS.md`
- `extra/rvv-vla/optimization/14_NATIVE_VPS_SUITE.md`

# RVV-VLA backend status

**Project:** simdjson RVV-VLA backend
**Baseline:** simdjson v4.6.11 (`f5de14f`)
**Branch:** `feature/rvv-vla`
**Last validated:** 2026-09-21
**Status:** correctness-complete for the current second-wave optimized snapshot under GCC/Clang + QEMU at VLEN=128/256/512/1024; native performance validation pending

This document records only results that have actually been observed. It is not a roadmap claim and it does not imply that the backend is production-ready or performance-validated.

## Current validated state

| Area | Status | Evidence / notes |
|---|---|---|
| RepoDiag doctor | PASS | RepoDiag detects the target repository and diagnostic levels correctly. |
| RVV static/source contract (`R10`) | PASS | Backend source layout, required integration points, tests and runner contract are present. |
| Windows -> WSL runner transition | PASS | Repository path translation is working with normalized `C:/...` paths and the Debian WSL distro. |
| Cross configuration | PASS | CMake configures with GCC 14.2.0 and Clang 19.1.7 RISC-V toolchains. |
| RVV backend compile | PASS | `libsimdjson.a` builds successfully with the scalable RVV backend enabled. |
| RVV test executables link | PASS | All 10 RVV-specific test executables link successfully. |
| QEMU execution | PASS | CTest launches RISC-V binaries through the configured QEMU cross-compiling emulator. |
| GCC global, VLEN=128 | **PASS** | 10/10 RVV tests pass under QEMU. |
| GCC global, VLEN=256 | **PASS** | 10/10 RVV tests pass under QEMU. |
| GCC global, VLEN=512 | **PASS** | 10/10 RVV tests pass under QEMU. |
| GCC global, VLEN=1024 | **PASS** | 10/10 RVV tests pass under QEMU. |
| GCC dispatch matrix | **PASS** | 10/10 RVV tests pass at VLEN=128/256/512/1024 with runtime-dispatch profile. |
| Clang global matrix | **PASS** | Clang 19.1.7: 10/10 RVV tests pass at VLEN=128/256/512/1024. |
| Upstream acceptance suite | **PASS** | GCC global: 89/89 tests pass at VLEN=128/256/512/1024. |
| RepoDiag `rvv-full` | **PASS with warnings** | 9 PASS / 2 WARN / 0 FAIL / 0 ERROR. RVV levels R10/R20/R30/R40/R50 pass. |
| RepoDiag `deep` | **PASS with warnings** | 10 PASS / 2 WARN / 0 FAIL / 0 ERROR. RVV levels R10/R20/R30/R40/R50 pass. |
| Real RISC-V hardware correctness | PENDING | QEMU is still the completed correctness environment for this snapshot. |
| Real hardware performance | PENDING | No performance claim is valid yet; native RVV 1.0 benchmarking is the next phase. |

## Environment used for the validated matrices

- Host: Windows with WSL2.
- WSL distribution: Debian GNU/Linux 13 (trixie).
- Python: 3.13.5 for the WSL test environment.
- CMake: 3.31.6.
- Ninja: 1.12.1.
- GCC RISC-V cross compiler: 14.2.0.
- Clang: 19.1.7.
- QEMU user-mode: 10.0.13.
- RVV compile target: `rv64gcv`, ABI `lp64d`.
- Validated QEMU vector lengths: 128, 256, 512 and 1024 bits.
- QEMU is used for correctness only, never for performance claims.

## Passing RVV test set

The following 10 tests pass in all currently validated GCC global, GCC dispatch and Clang global VLEN matrices:

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

Observed result for every validated VLEN/profile combination:

```text
100% tests passed, 0 tests failed out of 10
```

## Upstream acceptance validation

The GCC global acceptance suite has been executed at all four target VLEN values:

```text
VLEN=128   89/89 PASS
VLEN=256   89/89 PASS
VLEN=512   89/89 PASS
VLEN=1024  89/89 PASS
```

Observed result for every VLEN:

```text
100% tests passed, 0 tests failed out of 89
```

## Current optimized implementation state

The current snapshot includes the second performance-oriented optimization wave while preserving the scalable VLA design.

### Structural index writer

The Stage 1 structural writer now has three strategies:

1. ultra-sparse mask extraction for very small structural counts;
2. packed-mask writer using `vsm.v` -> scalar bitsets -> bit extraction -> `uint32_t` stores;
3. the previous dense `u32m8` `vid + vcompress` writer as a fallback.

The default thresholds remain configurable through RVV tuning macros, including:

- `SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD`
- packed-index threshold/configuration introduced for the second-wave writer.

This removes high-LMUL `vcompress` from more sparse and moderately sparse structural workloads without removing the dense vector fallback.

### Escape / backslash processing

The implementation contains:

- a rare-event path for very small backslash counts;
- packed mask serialization with `vsm.v` / scalar bitsets / `vlm.v`;
- reduced dependence on the older `vcompress + viota + vrgather` control path;
- the older path retained where appropriate as a fallback.

### Quote and inside-string processing

For applicable chunks, quote state processing uses packed mask words and scalar 64-bit prefix-XOR logic instead of the previous high-LMUL `viota` approach.

### Scalar-start propagation

Scalar-start propagation can use packed bitmask shift + carry instead of byte flags plus `vslide1up`.

### A/B controls

The packed-control optimizations have been split so that the major transformations can be benchmarked independently, while the umbrella packed-control setting remains available.

The native benchmark harness now supports a `phase1` VLA variant set intended to isolate the effects of:

- legacy control behavior;
- sparse structural indexing;
- packed structural indexing;
- sparse escape handling;
- packed escape handling;
- packed quote handling;
- packed scalar-start shift.

No variant is considered a performance winner until measured on real RVV hardware.

## Important fixes already validated

### Windows / WSL execution

The runner normalizes Windows paths before calling `wslpath` and pins the intended WSL distribution. This avoids losing backslashes when passing paths such as `C:\mycode\...` through `wsl.exe`.

### GCC scalable-vector type restrictions

GCC does not allow RVV sizeless types such as `vbool4_t` as ordinary struct members. Stage 1 keeps RVV mask values as local vector temporaries instead of stored object state.

### Generic number parsing integration

The RVV backend exposes the bit-manipulation helper required by generic number parsing (`leading_zeroes`).

### Stage 1 streaming integration

The RVV Stage 1 finish path references the generic streaming helpers in the namespace where they are defined.

### CTest cross-emulator handling

CMake test registration uses the target-aware `add_test(NAME ... COMMAND ...)` form so `CROSSCOMPILING_EMULATOR` is applied and the tests run through QEMU with the RISC-V sysroot.

### Differential-test oracle

For malformed JSON, different simdjson implementations can reject the same input with different error codes. Differential DOM tests therefore require agreement on acceptance vs rejection, and compare DOM values exactly only for successful parses.

For minification errors, output bytes / output length are not compared after an error; on successful minification the output remains compared exactly.

## Current non-blocking warnings

### simdjson `just_ascii` tooling warning

The current WSL environment does not have the `file` utility available to CMake, so simdjson reports:

```text
just_ascii test disabled because required tools were not found
```

This warning did not prevent the RVV matrices or the 89-test acceptance suite from passing.

### RepoDiag warnings

The latest `rvv-full` and `deep` campaigns contain no FAIL or ERROR results.

The remaining WARN conditions are non-RVV environment/repository hygiene items:

- `singleheader/singleheader.zip` is a large repository artifact (about 10.5 MB);
- `cargo` and `make` are not available on the Windows-side PATH used by RepoDiag.

These warnings do not currently block the RVV validation gates.

## Completed correctness gates

The planned pre-performance validation sequence is now complete:

```text
GCC global matrix       128 / 256 / 512 / 1024   PASS
GCC dispatch matrix     128 / 256 / 512 / 1024   PASS
Clang global matrix     128 / 256 / 512 / 1024   PASS
Acceptance GCC          89/89 x 4                PASS
RepoDiag rvv-full                                PASS with 2 WARN
RepoDiag deep                                    PASS with 2 WARN
```

The current snapshot can therefore be described as VLA-correct across the tested 128/256/512/1024-bit QEMU matrix and validated for runtime dispatch under the tested GCC configuration.

This does **not** establish a performance advantage over upstream `rvv_vls`.

## Next phase: native RVV performance validation

The next phase is native benchmarking on real RISC-V Vector 1.0 hardware.

First target:

- cfarm95 / Banana Pi BPI-F3
- SpacemiT K1 / X60
- VLEN=256

The benchmark must compare the current VLA backend against:

- the upstream `rvv_vls` backend;
- fallback;
- the isolated VLA A/B variants.

The primary workloads are:

- full DOM parse;
- Stage 1;
- Stage 2;
- minify;
- UTF-8 validation.

The first native A/B campaign is:

```text
python3 scripts/rvv/perf/bench_native.py all --profile quick --compiler gcc --vla-variants phase1
```

The purpose of this run is to identify which of the second-wave transformations are genuine wins on the K1/X60 before combining or extending them.

Only after native measurements should the project decide whether to:

- retune sparse/packed thresholds;
- fuse more of the Stage 1 control plane into persistent packed bitsets;
- test a wider `e8m4` Stage 1 configuration;
- further optimize UTF-8 or minify paths;
- remove or disable losing variants.

## Reproduction commands for the validated correctness gates

GCC global:

```text
python scripts/rvv/run_tests.py --compiler gcc --profile global --suite rvv --vlens 128,256,512,1024 --jobs 8
```

GCC runtime dispatch:

```text
python scripts/rvv/run_tests.py --compiler gcc --profile dispatch --suite rvv --vlens 128,256,512,1024 --jobs 8
```

Clang global:

```text
python scripts/rvv/run_tests.py --compiler clang --profile global --suite rvv --vlens 128,256,512,1024 --jobs 8
```

Upstream acceptance:

```text
python scripts/rvv/run_tests.py --compiler gcc --profile global --suite acceptance --vlens 128,256,512,1024 --jobs 8
```

RepoDiag `rvv-full`:

```text
python ..\RepoDiagSimdjson\repodiag.py --target C:\mycode\Simdjson\simdjson-rvv-vla run rvv-full --jobs 8
```

RepoDiag `deep`:

```text
python ..\RepoDiagSimdjson\repodiag.py --target C:\mycode\Simdjson\simdjson-rvv-vla run deep --jobs 8
```

## Status policy

Update this file only when a gate has actually been executed.

Use:

- **PASS** — command completed successfully and the expected tests ran.
- **FAIL** — command ran and produced a reproducible failure.
- **BLOCKED** — prerequisite or environment prevents the gate from running.
- **PENDING** — not yet executed.

Do not turn QEMU correctness results into performance claims. Do not turn one VLEN result into a claim of full VLA coverage. Native hardware measurements are required before describing the backend as performance-competitive or optimal.

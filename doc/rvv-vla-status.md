# RVV-VLA backend status

**Project:** simdjson RVV-VLA backend  
**Baseline:** simdjson v4.6.11 (`f5de14f`)  
**Branch:** `feature/rvv-vla`  
**Last validated:** 2026-09-20  
**Status:** first end-to-end GCC/QEMU validation passing at VLEN=128

This document records only results that have actually been observed. It is not a roadmap claim and it does not imply that the backend is production-ready or performance-validated.

## Current validated state

| Area | Status | Evidence / notes |
|---|---|---|
| RepoDiag doctor | PASS | RepoDiag detects the target repository and diagnostic levels correctly. |
| RVV static/source contract (`R10`) | PASS | Backend source layout, required integration points, tests and runner contract are present. |
| Windows -> WSL runner transition | PASS | Repository path translation is working with normalized `C:/...` paths and the Debian WSL distro. |
| Cross configuration | PASS | CMake configures with `riscv64-linux-gnu-g++` / GCC 14.2.0. |
| RVV backend compile | PASS | `libsimdjson.a` builds successfully with the scalable RVV backend enabled. |
| RVV test executables link | PASS | All 10 RVV-specific test executables link successfully. |
| QEMU execution | PASS | CTest launches RISC-V binaries through the configured QEMU cross-compiling emulator. |
| GCC global, VLEN=128 | **PASS** | 10/10 RVV tests pass under QEMU. |
| GCC global, VLEN=256 | PENDING | Not yet validated. |
| GCC global, VLEN=512 | PENDING | Not yet validated. |
| GCC global, VLEN=1024 | PENDING | Not yet validated. |
| GCC dispatch matrix | PENDING | Runtime-dispatch profile not yet validated. |
| Clang global matrix | PENDING | Clang 19.1.7 is installed; validation not yet run. |
| Upstream acceptance suite | PENDING | Not yet run for this backend state. |
| RepoDiag `rvv-full` | PENDING | Run after compiler and acceptance matrices. |
| RepoDiag `deep` | PENDING | Final diagnostic campaign, not yet run. |
| Real RISC-V hardware correctness | PENDING | QEMU is the current correctness environment. |
| Real hardware performance | PENDING | No performance claims are valid yet. |

## Environment used for the first passing end-to-end run

- Host: Windows with WSL2.
- WSL distribution: Debian GNU/Linux 13 (trixie).
- Python: 3.13.5.
- CMake: 3.31.6.
- Ninja: 1.12.1.
- Cross compiler: `riscv64-linux-gnu-g++` 14.2.0.
- QEMU user-mode: 10.0.13.
- Clang available: 19.1.7.
- RVV global compile profile: `rv64gcv`, ABI `lp64d`.
- Validated QEMU vector length: 128 bits.

## Passing RVV test set at VLEN=128

The following tests all pass in the GCC global profile:

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

Observed result:

```text
100% tests passed, 0 tests failed out of 10
```

## Important fixes already validated

### Windows / WSL execution

The runner now normalizes Windows paths before calling `wslpath` and pins the intended WSL distribution. This avoids losing backslashes when passing a path such as `C:\mycode\...` through `wsl.exe`.

### GCC scalable-vector type restrictions

GCC does not allow RVV sizeless types such as `vbool4_t` as ordinary struct members. Stage 1 was adjusted so RVV mask values remain local vector temporaries instead of stored object state.

### Generic number parsing integration

The RVV backend now exposes the bit-manipulation helper required by generic number parsing (`leading_zeroes`).

### Stage 1 streaming integration

The RVV Stage 1 finish path now references the generic streaming helpers in the namespace where they are actually defined.

### CTest cross-emulator handling

The CMake test registration uses the target-aware `add_test(NAME ... COMMAND ...)` form so `CROSSCOMPILING_EMULATOR` is applied and the tests run through QEMU with the RISC-V sysroot.

### Differential-test oracle

For malformed JSON, different simdjson implementations can reject the same input with different error codes. Differential DOM tests therefore require agreement on acceptance vs rejection, and compare DOM values exactly only for successful parses.

For minification errors, output bytes / output length are not compared after an error; on successful minification the output remains compared exactly.

## Known non-blocking environment warning

The current WSL environment does not have the `file` utility installed, so simdjson reports:

```text
just_ascii test disabled because required tools were not found
```

This does not affect the RVV-specific VLEN=128 suite, but it should be fixed before the full upstream acceptance campaign.

## Next validation gates

Run these in order and stop at the first failure:

```text
GCC global matrix       128 / 256 / 512 / 1024
GCC dispatch matrix     128 / 256 / 512 / 1024
Clang global matrix
Acceptance GCC
RepoDiag rvv-full
RepoDiag deep
```

The backend should not be described as broadly VLA-validated until the GCC global matrix passes at 128, 256, 512 and 1024 bits.

The backend should not be described as runtime-portable until the dispatch matrix passes.

The backend should not be described as performance-competitive or optimal until benchmarks are run on real RVV 1.0 hardware against the official `rvv-vls` backend under controlled compiler, corpus and build settings.

## Reproduction command for the validated gate

From the Windows control panel, the passing gate corresponds to:

```text
python scripts/rvv/run_tests.py --compiler gcc --profile global --suite rvv --vlens 128 --jobs 8
```

On Windows the runner re-enters Debian WSL automatically.

## Status policy

Update this file only when a gate has actually been executed.

Use:

- **PASS** — command completed successfully and the expected tests ran.
- **FAIL** — command ran and produced a reproducible failure.
- **BLOCKED** — prerequisite or environment prevents the gate from running.
- **PENDING** — not yet executed.

Do not turn QEMU correctness results into performance claims. Do not turn one VLEN result into a claim of full VLA coverage.

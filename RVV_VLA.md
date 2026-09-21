# simdjson RVV-VLA backend

This repository snapshot is the complete simdjson `v4.6.11` source tree with the
scalable RISC-V Vector (`rvv`) backend, its tests, diagnostics, and development
specification integrated in place.

## Baseline and identity

- Upstream baseline: simdjson `v4.6.11`
- Baseline commit: `f5de14f09256982933af2849beb43778bd421ca7`
- Backend implementation key: `rvv`
- Namespace: `simdjson::rvv`
- Existing upstream fixed-vector backend retained: `rvv_vls`
- Target ISA model: RV64 + RISC-V Vector 1.0, VLA/scalable semantics

The `rvv` backend is separate from `rvv_vls`. The latter is
Vector-Length-Specific; the new backend is intended to scale with runtime VLEN
without compile-time fixed-vector-bit assumptions in its hot core.

## Production code

Primary implementation paths:

- `include/simdjson/rvv.h`
- `include/simdjson/rvv/`
- `src/rvv.cpp`
- `src/rvv/`
- `src/internal/isadetection.h`
- `include/simdjson/internal/instruction_set.h`
- `include/simdjson/implementation_detection.h`
- `include/simdjson/portability.h`
- builtin implementation switchboards
- `singleheader/simdjson.h`
- `singleheader/simdjson.cpp`

The implementation covers scalable Stage 1, quote/backslash state, direct
structural-index emission, UTF-8 validation, string-aware minification,
Stage 2 adapters, DOM/On-Demand integration, number/string parsing glue,
runtime dispatch, and single-header integration.

## Canonical specification

The complete v1.1 development specification is stored in:

`extra/rvv-vla/spec/`

Start with `extra/rvv-vla/spec/00_START_HERE.md`.

Especially relevant:

- `03_LOCKED_ARCHITECTURE.md`
- `04_STAGE1_KERNEL.md`
- `05_UTF8_MINIFY_STAGE2.md`
- `06_RUNTIME_DISPATCH_BUILD_AMALGAMATION.md`
- `07_SOURCE_LAYOUT_AND_MIGRATION.md`
- `08_CORRECTNESS_TESTING_FUZZING.md`
- `11_AI_CODING_PROTOCOL.md`
- `20_ALGORITHM_INVARIANTS.md`
- `21_PERFORMANCE_HYPOTHESES.md`
- `23_BENCHMARK_EXPERIMENT_MATRIX.md`
- `24_PROJECT_STATE.md`

## RVV-specific tests

Backend-specific C++ tests live in `tests/rvv/` and are opt-in through
`SIMDJSON_RVV_TESTS`.

They cover registry/dispatch, DOM equivalence, Stage 1 boundaries, minify,
UTF-8, number parsing, On-Demand, streaming, randomized differential parsing,
and corpus parsing.

## Cross-build and QEMU

Main runner:

`python3 scripts/rvv/run_tests.py`

Toolchains:

- `cmake/toolchains/rvv-vla-qemu-gcc.cmake`
- `cmake/toolchains/rvv-vla-qemu-clang.cmake`

The runner supports VLEN 128, 256, 512 and 1024 correctness runs and both
global-RVV and runtime-dispatch profiles.

## External diagnostics

Repository-wide orchestration is intentionally kept outside this repository.
The companion generic diagnostics repository is **RepoDiag**. It can target this
repo with `--target` and invoke the native RVV runner/tests without vendoring the
diagnostics framework into simdjson.

The product repository itself remains self-contained for build and correctness:
`tests/rvv/`, `scripts/rvv/`, the CMake toolchains, and the full design spec are
all kept here.

## Documentation map

Project-specific documentation:

- `RVV_VLA.md`
- `doc/rvv-vla-architecture.md`
- `doc/rvv-vla-testing.md`
- `doc/rvv-vla-source-map.md`
- `extra/rvv-vla/README.md`
- `extra/rvv-vla/spec/`
- `tests/rvv/README.md`

The original simdjson documentation remains under `doc/`.

## Performance policy

QEMU is for correctness, not performance claims. Performance decisions should be
validated on physical RVV 1.0 hardware, comparing `rvv` against upstream
`rvv_vls` with the same compiler, flags, corpora and measurement method.

## AI-assisted development

The repository retains simdjson's `AI_USAGE_POLICY.md`. Significant
AI-assisted changes should be reviewed and understood by the human contributor
before publication or upstream submission.

# RVV-VLA source map

## Backend declaration

- `include/simdjson/rvv.h`
- `include/simdjson/rvv/base.h`
- `include/simdjson/rvv/begin.h`
- `include/simdjson/rvv/end.h`
- `include/simdjson/rvv/implementation.h`
- `include/simdjson/rvv/intrinsics.h`
- `include/simdjson/rvv/stringparsing_defs.h`
- `include/simdjson/rvv/numberparsing_defs.h`
- `include/simdjson/rvv/ondemand.h`
- `include/simdjson/rvv/builder.h`

## Hot implementation

- `src/rvv.cpp`
- `src/rvv/stage1.h`
- `src/rvv/string_scanner.h`
- `src/rvv/utf8_validation.h`
- `src/rvv/minify.h`

## Dispatch and registry

- `include/simdjson/implementation_detection.h`
- `include/simdjson/internal/instruction_set.h`
- `include/simdjson/portability.h`
- `include/simdjson/builtin.h`
- `include/simdjson/builtin/base.h`
- `include/simdjson/builtin/implementation.h`
- `include/simdjson/builtin/ondemand.h`
- `include/simdjson/builtin/builder.h`
- `include/simdjson/generic/base.h`
- `src/internal/isadetection.h`
- `src/implementation.cpp`
- `src/simdjson.cpp`

## Single-header

- `singleheader/amalgamate.py`
- `singleheader/simdjson.h`
- `singleheader/simdjson.cpp`

## Correctness validation

- `tests/rvv/`
- `scripts/rvv/run_tests.py`
- `cmake/toolchains/rvv-vla-qemu-gcc.cmake`
- `cmake/toolchains/rvv-vla-qemu-clang.cmake`

`scripts/rvv/run_tests.py` supports both the cross/QEMU matrix and native
`riscv64` correctness execution. On native hosts it uses host `gcc/g++` or
`clang/clang++`, with optional `RVV_NATIVE_*` compiler overrides.

## Native VPS orchestration

- `scripts/rvv/native_suite.py`

This is the recommended one-command entry point for a new real RVV host. It
collects environment metadata, runs compiler/VLEN preflights, selected native
correctness gates and the native performance harness, then produces a compact
result bundle.

## Native performance lab

- `scripts/rvv/perf/bench_native.py`
- `scripts/rvv/perf/probe_vlen.cpp`
- `scripts/rvv/perf/rvv_microbench.cpp`
- `scripts/rvv/perf/README.md`

The performance lab builds current RVV-VLA, fallback, pristine upstream
RVV-VLS variants and optional isolated VLA A/B candidates. It records raw CSV,
summary CSV, metadata and a Markdown report.

## Documentation

- `doc/rvv-vla-architecture.md`
- `doc/rvv-vla-source-map.md`
- `doc/rvv-vla-status.md`
- `doc/rvv-vla-testing.md`

## Specification

- `extra/rvv-vla/spec/`

## Optimization plan and experiments

- `extra/rvv-vla/optimization/00_START_HERE.md`
- `extra/rvv-vla/optimization/13_PACKED_INDEX_AND_AB_HARNESS.md`
- `extra/rvv-vla/optimization/14_NATIVE_VPS_SUITE.md`

## External diagnostics

- `RepoDiag` (separate repository; optional orchestration only)

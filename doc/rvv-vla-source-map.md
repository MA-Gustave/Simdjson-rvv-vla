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

## Validation

- `tests/rvv/`
- `scripts/rvv/run_tests.py`
- `cmake/toolchains/rvv-vla-qemu-gcc.cmake`
- `cmake/toolchains/rvv-vla-qemu-clang.cmake`

## Specification

- `extra/rvv-vla/spec/`

## External diagnostics

- `RepoDiag` (separate repository; optional orchestration only)

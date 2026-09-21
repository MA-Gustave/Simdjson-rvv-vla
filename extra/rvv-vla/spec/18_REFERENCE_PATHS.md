# Reference Paths and Evidence Map

## Frozen target: simdjson v4.6.11

### RVV-VLS reference

```text
include/simdjson/rvv-vls.h
include/simdjson/rvv-vls/*
src/rvv-vls.cpp
```

### Generic Stage 1 semantics

```text
src/generic/stage1/json_structural_indexer.h
src/generic/stage1/json_scanner.h
src/generic/stage1/json_string_scanner.h
src/generic/stage1/json_escape_scanner.h
src/generic/stage1/json_minifier.h
src/generic/stage1/utf8_lookup4_algorithm.h
src/generic/stage1/find_next_document_index.h
src/generic/stage1/buf_block_reader.h
```

The frozen `json_structural_indexer.h` is especially important for capacity, streaming, sentinel and exact index-output semantics.

### Vector structural-writer precedent

```text
src/icelake.cpp
```

Look for `SIMDJSON_GENERIC_JSON_STRUCTURAL_INDEXER_CUSTOM_BIT_INDEXER` as precedent for architecture-specific structural emission. Do not infer that the same abstraction must be retained in the custom VLA backend.

### Dispatch/build

```text
include/simdjson/implementation_detection.h
include/simdjson/portability.h
include/simdjson/internal/instruction_set.h
include/simdjson/builtin/implementation.h
src/internal/isadetection.h
src/implementation.cpp
src/simdjson.cpp
singleheader/amalgamate.py
```

### Stage 2

```text
src/generic/stage2/stringparsing.h
src/generic/stage2/tape_builder.h
src/generic/stage2/*
```

## Old RVV branch assets worth mining

```text
tests/rvv/*
fuzz/rvv_fuzz_*.cpp
scripts/rvv/*
cmake/toolchains/rvv-qemu-*.cmake
tools/rvv/*
benchmark/rvv/*
benchmarks/rvv_*
extra/rvv/*
doc/rvv.md
```

Treat old architectural locks and hot-core implementations as historical unless independently revalidated.

## External public references

- simdjson issue #2423 — RVV proposal/discussion
- simdjson draft PR #2442 — VLS vs VLA design discussion
- simdjson PR #2593 — upstream `rvv_vls`, with future VLA plan
- simdjson PR #2617 — localized dynamic-`vsetvl` RVV path
- old experimental PR #2583 — January backend
- Linux RISC-V hwprobe documentation
- GCC current RISC-V attributes documentation (`target("arch=...")` / `#pragma GCC target`)
- simdutf current RVV implementation, especially UTF-8 validation
- RISC-V Vector intrinsic/specification documentation matching the compiler toolchain

Record source commit/date and license before adapting external code. If a public source has changed since the spec freeze, log that fact in `24_PROJECT_STATE.md` before changing architecture.

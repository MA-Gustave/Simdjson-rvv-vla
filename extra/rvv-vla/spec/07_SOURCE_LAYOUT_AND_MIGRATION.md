# Source Layout and Migration Plan

## 1. Target core tree

The final backend should converge on this logical layout. Exact consolidation is allowed only when it preserves responsibilities.

```text
include/simdjson/rvv.h
include/simdjson/rvv/base.h
include/simdjson/rvv/begin.h
include/simdjson/rvv/end.h
include/simdjson/rvv/implementation.h
include/simdjson/rvv/intrinsics.h
include/simdjson/rvv/stage1.h
include/simdjson/rvv/string_scanner.h
include/simdjson/rvv/utf8_validation.h
include/simdjson/rvv/minify.h
include/simdjson/rvv/stringparsing_defs.h
include/simdjson/rvv/numberparsing_defs.h
include/simdjson/rvv/ondemand.h
include/simdjson/rvv/builder.h          # if required by current tree
src/rvv.cpp
```

## 2. Responsibilities

### `base.h`

- namespace declarations/support;
- RVV target macros used by the backend;
- no hot algorithm code.

### `intrinsics.h`

- central direct include of `<riscv_vector.h>` where practical;
- compatibility wrappers only when compiler spelling genuinely differs;
- no x86-like pseudo-SIMD abstraction layer.

### `stage1.h`

- custom VLA Stage 1 state and main scanning kernel;
- classification;
- scalar-start logic;
- structural index emission;
- integration hooks needed by `src/rvv.cpp`.

### `string_scanner.h`

- escape-run processing;
- unescaped quote mask;
- inside-string prefix parity;
- reusable state helper for Stage 1/minify.

### `utf8_validation.h`

- standalone validator;
- Stage 1 validation helper/state.

### `minify.h`

- string-aware whitespace removal using shared string-state logic and `vcompress`.

### `stringparsing_defs.h`

- 64-byte semantic VLA adapter for generic Stage 2 string parsing.

### `numberparsing_defs.h`

- current proven SWAR primitives; no speculative vector rewrite.

### `implementation.h`

- `simdjson::rvv::implementation` metadata and required instruction-set bit.

### `src/rvv.cpp`

- parser factory;
- generic includes and Stage 2 glue;
- public implementation methods;
- target-region boundary if used;
- minimal code that cannot live cleanly in headers.

## 3. Old core file disposition

### Rewrite from scratch against this spec

```text
include/simdjson/rvv/simd.h
include/simdjson/rvv/bitmask.h
include/simdjson/rvv/stage1.h
include/simdjson/rvv/minify.h
include/simdjson/rvv/utf8_validation.h
src/rvv.cpp
```

The old versions encode the bring-up architecture and must not be mechanically copied.

### Adapt carefully

```text
include/simdjson/rvv.h
base.h
begin.h
end.h
implementation.h
intrinsics.h
stringparsing_defs.h
numberparsing_defs.h
ondemand.h
builder.h
```

Reuse naming/integration patterns, but align them with v4.6.11 and current `rvv_vls` structure.

### Likely delete / do not recreate unless needed

```text
rvv_config.h      # old locked 64B/m1 policy is obsolete
load_store.h      # only keep if it earns a clear focused role
stage2.h          # generic Stage 2 should normally make this unnecessary
old bitmask abstraction whose purpose is scalar uint64 mask export
```

## 4. Old test/tool assets

### Keep and update

```text
tests/rvv/rvv_compilation_tests.cpp
tests/rvv/rvv_stage1_tests.cpp
tests/rvv/rvv_stage2_tests.cpp
tests/rvv/rvv_utf8_tests.cpp
tests/rvv/rvv_minify_tests.cpp
tests/rvv/rvv_dom_tests.cpp
tests/rvv/rvv_ondemand_tests.cpp
tests/rvv/rvv_number_tests.cpp
tests/rvv/rvv_equivalence_tests.cpp
tests/rvv/rvv_regression_tests.cpp
tests/rvv/rvv_corpus_tests.cpp
```

Update implementation-selection assumptions and add VLA boundary tests.

### Keep fuzz concepts

```text
fuzz/rvv_fuzz_parse.cpp
fuzz/rvv_fuzz_minify.cpp
fuzz/rvv_fuzz_validate_utf8.cpp
```

Prefer differential fuzzing against fallback.

### Consolidate benchmarks

The old branch has both `benchmark/rvv/*` and `benchmarks/rvv_*`. Keep one canonical location consistent with current simdjson's build system.

Required benchmark programs:

```text
bench_stage1
bench_minify
bench_utf8
bench_end_to_end / existing simdjson benchmark integration
```

`bench_numbers` is optional because number parsing is not the primary optimization target.

## 5. Scripts/toolchains

Retain concepts from:

```text
cmake/toolchains/rvv-qemu-clang.cmake
cmake/toolchains/rvv-qemu-gcc.cmake
scripts/rvv/common.*
scripts/rvv/build_rvv_*.sh
scripts/rvv/run_qemu_ctest.sh
scripts/rvv/run_qemu_smoke.sh
scripts/rvv/run_qemu_bench.sh
```

But rename/update build directories and compiler versions as needed. Do not preserve stale absolute Windows paths from old docs/comments.

## 6. Documentation migration

Old `extra/rvv/*` docs are historical input. This spec replaces their architecture locks.

Useful old material to preserve in updated form:

- build troubleshooting;
- QEMU command knowledge;
- CI conventions;
- debugging methods;
- upstreaming checklist;
- benchmark methodology.

The old `STORE_AND_PACK_U64`, `LMUL=m1`, and universal `RVV_BLOCK_BYTES=64` rules must be marked obsolete wherever encountered.

## 7. Existing upstream files expected to change

Minimal switchboard set, derived from v4.6.11:

```text
include/simdjson/implementation_detection.h
include/simdjson/internal/instruction_set.h
include/simdjson/portability.h
src/internal/isadetection.h
src/implementation.cpp
src/simdjson.cpp
include/simdjson/builtin.h
include/simdjson/builtin/base.h
include/simdjson/builtin/implementation.h
include/simdjson/builtin/ondemand.h
include/simdjson/builtin/builder.h       # if required
singleheader/amalgamate.py               # if required
build/CMake integration files            # exact location verified in target tree
```

Only touch a listed switchboard file if the target tree actually requires it.

## 8. Files explicitly not to copy blindly

The old commit that added generic On-Demand/builder files was not RVV-specific. Do not overwrite current v4.6.11 generic files from the old branch.

Any generic-file edit must be justified by a backend-neutral requirement or a small, explicit extension point.

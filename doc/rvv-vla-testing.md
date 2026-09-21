# RVV-VLA testing and diagnostics

## Native simdjson tests

The complete upstream simdjson test suite remains in the repository.

The dedicated RVV suite is in `tests/rvv/` and is enabled with:

`-DSIMDJSON_RVV_TESTS=ON`

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

## QEMU matrix

Use:

`python3 scripts/rvv/run_tests.py`

Supported correctness VLEN values are 128, 256, 512 and 1024.

Toolchain files:

- `cmake/toolchains/rvv-vla-qemu-gcc.cmake`
- `cmake/toolchains/rvv-vla-qemu-clang.cmake`

QEMU is a correctness environment only, not a performance oracle.

## External RepoDiag orchestration

The optional generic diagnostics/orchestration tool is maintained in the separate
**RepoDiag** repository. Point it at this repository explicitly, for example:

`python repodiag.py --target C:\mycode\Simdjson\simdjson-rvv-vla run rvv`

RepoDiag delegates RVV cross-build and QEMU execution to
`scripts/rvv/run_tests.py`; the authoritative tests and runner remain in this
repository. This keeps upstream-facing simdjson changes independent of the
diagnostics framework.

## Real hardware

Performance validation belongs on physical RVV 1.0 hardware.

See:

- `extra/rvv-vla/spec/09_PERFORMANCE_BENCHMARKING.md`
- `extra/rvv-vla/spec/23_BENCHMARK_EXPERIMENT_MATRIX.md`

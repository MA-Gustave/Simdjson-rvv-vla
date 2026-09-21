# RVV-VLA project area

This directory contains the design, validation, and performance-engineering material for the scalable RISC-V Vector (`rvv`) backend.

## Canonical design

`spec/` contains the canonical v1.1 design specification.

Use the specification for architectural invariants, correctness requirements, backend scope, and the original implementation plan.

## Performance optimization

`optimization/` contains the performance-optimization series for the next phase of the project.

Start with:

- `optimization/00_START_HERE.md`

The series documents:

- the current correctness-first hotspots;
- relevant RVV microarchitecture evidence, including SpacemiT X60/K1 results;
- the proposed Stage 1 fast pipeline;
- packed mask / bitset prefix processing;
- structural-index writer redesign;
- quote, backslash, and string-state optimization;
- UTF-8 validation optimization;
- minify optimization;
- LMUL/VLEN performance-portability rules;
- the native benchmark and promotion protocol;
- the ordered experiment queue.

The central optimization direction is to keep byte processing wide while moving irregular control work away from expensive high-LMUL permutation sequences where measurements justify it. In particular, the optimization work will test packed-mask alternatives to high-LMUL `vcompress`, `vrgather`, `viota`, and element-slide-heavy control paths.

Performance claims must be based on real RVV hardware. QEMU remains a correctness tool.

## Production backend

The production backend deliberately lives in normal simdjson source locations:

- `include/simdjson/rvv.h`
- `include/simdjson/rvv/`
- `src/rvv.cpp`
- `src/rvv/`

The existing fixed-vector upstream comparison backend is:

- `src/rvv-vls.cpp`
- `include/simdjson/rvv-vls/`

## Validation and benchmark support

Supporting validation and tooling lives in:

- `tests/rvv/`
- `scripts/rvv/`
- `scripts/rvv/perf/`
- `cmake/toolchains/`
- `repodiag/`

The native performance harness under `scripts/rvv/perf/` is the reference path for comparing the current RVV-VLA backend against upstream `rvv_vls` on physical RVV hardware.

Before changing performance-critical algorithms, record and preserve a native baseline.

## Suggested reading order

1. Repository-root `RVV_VLA.md` — complete project map.
2. `spec/` — canonical v1.1 design.
3. `optimization/00_START_HERE.md` — performance-phase overview.
4. `optimization/02_CURRENT_HOTSPOTS.md` — current likely bottlenecks.
5. `optimization/11_EXPERIMENT_QUEUE.md` — concrete optimization order.
6. `scripts/rvv/perf/README.md` — native benchmark procedure.

Correctness remains the gate for every optimization. Performance experiments should be introduced one at a time so gains and regressions can be attributed to a specific change.

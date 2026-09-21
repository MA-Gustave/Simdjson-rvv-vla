# RVV-VLA Performance Optimization Plan — Start Here

## Purpose

This document set defines how to optimize the `rvv` vector-length-agnostic backend in
`MA-Gustave/Simdjson-rvv-vla` after correctness has been established.

The optimization target is not merely to make a scalable RVV backend work. The target is
to make it competitive with, and where possible faster than, the existing fixed-vector
`rvv_vls` backend on real RVV 1.0 hardware while preserving the architectural advantages
of VLA:

- one backend for multiple runtime VLENs;
- runtime dispatch;
- no compile-time fixed-vector-length requirement in the hot core;
- correctness across VLEN 128/256/512/1024;
- performance portability across multiple RVV microarchitectures.

The current backend is a **correctness-first implementation**. Its present instruction
selection must not be mistaken for the final performance architecture.

## Current validated baseline

The current backend has already passed:

- GCC global-RVV correctness at VLEN 128/256/512/1024;
- GCC runtime-dispatch correctness at VLEN 128/256/512/1024;
- Clang global-RVV correctness at VLEN 128/256/512/1024;
- upstream acceptance tests across the VLEN matrix;
- RepoDiag RVV source contract, GCC matrix, dispatch matrix, Clang matrix and upstream
  acceptance.

QEMU remains a correctness tool only. Performance decisions require physical RVV hardware.

## Primary performance baseline

The first primary machine should be a SpacemiT X60 / K1 with RVV 1.0 and VLEN=256,
such as a Banana Pi BPI-F3 or equivalent remote system.

The existing native harness under:

```text
scripts/rvv/perf/
```

should be used before changing the algorithm. Preserve a raw baseline for:

- `rvv` current VLA;
- `rvv_vls` at the native fixed VLEN;
- fallback;
- Stage 1;
- DOM full parse;
- Stage 2 control;
- minify;
- UTF-8 validation.

## Main finding from external RVV research

Olaf Bernstein (`camel-cdr`), author of simdjson's upstream `rvv_vls` backend, maintains
a large RVV benchmark suite and real-hardware result database. His measurements show that
on the SpacemiT X60, high-LMUL permutation operations can become dramatically more
expensive as LMUL grows.

This is directly relevant to the present backend, which currently uses operations such as:

- `vcompress` on wide index vectors;
- `vrgather` in escape processing;
- `viota` for quote/escape prefix work;
- vector element slides where the logical operation is really a mask shift.

The key optimization direction is therefore:

> **Keep wide byte processing, but move irregular control information into compact
> mask/bitset form as early as practical. Avoid scaling expensive permutation machinery
> with LMUL.**

This is the central thesis of this document set.

## Recommended optimization sequence

Do not implement all ideas at once. Preserve attribution of wins and regressions.

1. Record native baseline.
2. Add microbenchmarks for the exact primitives used by Stage 1.
3. Replace the structural-index writer.
4. Replace sparse backslash handling.
5. Replace quote-prefix / string-state handling where profitable.
6. Remove unnecessary element slides used only to move predicates.
7. Re-test `e8m2` versus wider byte processing such as `e8m4`.
8. Tune UTF-8 and minify independently.
9. Validate on a second RVV microarchitecture before promoting hardware-specific choices
   into the portable default.

## Files in this series

- `01_EVIDENCE_AND_CONSTRAINTS.md` — evidence hierarchy and external research.
- `02_CURRENT_HOTSPOTS.md` — code-level map of current likely bottlenecks.
- `03_STAGE1_FAST_PIPELINE.md` — target Stage 1 architecture.
- `04_MASKS_BITSETS_PREFIX.md` — scalable mask/bitset strategy.
- `05_STRUCTURAL_INDEX_WRITER.md` — structural-index emission redesign.
- `06_QUOTES_BACKSLASHES_STRINGS.md` — quote, escape and string-state redesign.
- `07_UTF8_VALIDATION.md` — UTF-8 optimization plan.
- `08_MINIFY.md` — minify optimization plan.
- `09_LMUL_VLEN_PORTABILITY.md` — LMUL, VLEN and cross-hardware policy.
- `10_BENCHMARK_PROMOTION_PROTOCOL.md` — how a candidate becomes production code.
- `11_EXPERIMENT_QUEUE.md` — concrete experiment order and decision tree.
- `12_FIRST_WAVE_IMPLEMENTED.md` — implemented optimization candidates awaiting native promotion evidence.
- `13_PACKED_INDEX_AND_AB_HARNESS.md` — packed structural writer + isolated native A/B variants.
- `SOURCES.md` — external references used by this plan.

## Non-goals

This plan does not assume a particular optimization is a win before measurement.
In particular:

- `e8m4` is not automatically faster than `e8m2`;
- scalar bitset work is not automatically faster than vector mask work;
- `vcompress` is not universally bad;
- the X60 is not representative of every future RVV CPU;
- an optimization that wins only a synthetic microbenchmark is not sufficient.

The final backend should be chosen by measured end-to-end behavior, not by elegance,
instruction count, or theoretical vector width.

# Product Requirements

## 1. Goal

Deliver a complete RVV 1.0 VLA backend for simdjson that can be selected as `rvv`, scales across hardware vector lengths, preserves simdjson public behavior, and has a reproducible performance case against upstream `rvv_vls`.

This is performance-driven engineering. Portability alone is valuable, but the intended upstream story is stronger: a scalable backend with a measurable benefit or a compelling performance/portability trade-off.

## 2. User-visible outcome

On an RVV-capable Linux system, a supported simdjson library build containing `rvv` should be able to select it safely without requiring the entire application to be globally compiled for one fixed hardware VLEN.

The public simdjson parsing API remains unchanged.

## 3. Required functional scope

### Stage 1

The backend must correctly implement:

- JSON whitespace and structural-operator classification;
- quote and backslash recognition;
- exact escape handling across VLA chunk boundaries;
- exact inside-string state across chunk boundaries;
- pseudo-structural scalar-start detection;
- unescaped control-character detection inside strings;
- structural index emission into the existing `uint32_t` buffer;
- regular, streaming-partial, and streaming-final semantics;
- parser UTF-8 validation behavior.

### Stage 2 and parser behavior

The backend must support DOM, On-Demand, document streams, string parsing/unescaping, number parsing, parser creation, Stage 2 and Stage 2-next. Generic Stage 2 reuse is expected unless profiling later proves a separate hotspot.

### Utility APIs

The backend must provide correct `validate_utf8`, `minify`, implementation registration, force-selection behavior, and runtime support checks.

## 4. Performance objective

The primary competitor is `rvv_vls`; fallback is context only.

Required measurements include Stage 1 throughput, cycles/byte, instructions/byte where reliable, UTF-8, minify, DOM and On-Demand end-to-end results.

No speed claim may be based on QEMU timing.

## 5. Success levels

### Minimum technical success

- full correctness matrix passes;
- one VLA backend design runs at VLEN 128/256/512/1024 under QEMU correctness testing;
- safe supported runtime/build mode exists;
- no fixed-vector-bit requirement exists in the VLA Stage 1 core;
- real-hardware Stage 1 is not materially slower than `rvv_vls` on the primary target after tuning.

### Publishable performance success

At least one of the following on real hardware with repeatable measurements:

- Stage 1 geometric-mean improvement of at least 15% versus `rvv_vls` over the agreed corpus;
- at least 20% improvement on two representative workloads without severe regressions elsewhere;
- at least 10% end-to-end parser improvement on a representative workload with an ablation-supported explanation;
- or a smaller speed gain accompanied by a clearly demonstrated deployment advantage that `rvv_vls` cannot provide, subject to maintainer interest.

These are project gates, not promised results.

### Strong result

Repeat the result on two distinct RVV machines/generations, ideally with different VLENs, while preserving the single scalable backend property.

## 6. Performance hypotheses are not requirements

The following are **not** product requirements by themselves:

- LMUL=m1, m2, m4, or any exact LMUL;
- `viota` as the quote-parity implementation;
- `viota/vrgather` as the escape algorithm;
- unconditional `vcompress` for every structural density;
- mandatory fused UTF-8 in the same register schedule;
- any fixed threshold for adaptive paths.

Those choices live in `21_PERFORMANCE_HYPOTHESES.md` and are selected by evidence.

## 7. Kill / rethink criteria

Reconsider the architecture when, after correctness and representative tuning:

- mature Stage 1 remains more than 10% slower than `rvv_vls` on ordinary JSON;
- structural emission cannot match the scalar/reference alternative on relevant densities;
- string/escape handling dominates enough to erase the expected benefit;
- safe supported runtime dispatch cannot be achieved without disproportionate complexity;
- code complexity is materially larger than the measured value it creates.

A negative result is acceptable. An unsupported positive narrative is not.

## 8. Non-goals

Do not delete `rvv_vls`, rewrite unrelated simdjson code, add a new parsing API, vectorize Stage 2 for novelty, support pre-1.0 vendor vector ISAs, use QEMU as a speed oracle, or claim universal superiority.

## 9. Compatibility constraints

- core source is ASCII;
- public macros use `SIMDJSON_`;
- no core stdout/stderr, `abort()` or `exit()`;
- non-RISC-V builds never require `<riscv_vector.h>`;
- sizeless RVV types do not cross ABI/public-data boundaries;
- C++ usage remains compatible with simdjson's supported baseline.

## 10. Human/AI accountability

All AI-generated code/text intended for contribution must be read, understood and reviewed by the human contributor. The human is the author and must be able to explain the algorithms, tests and measurements. This mirrors the upstream `AI_USAGE_POLICY.md` present in the frozen target tree.

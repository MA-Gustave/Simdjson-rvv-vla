# simdjson RVV-VLA Upgrade Specification

**Spec version:** 1.1
**Frozen development baseline:** simdjson `v4.6.11`
**Target implementation key:** `rvv`
**Reference implementation:** upstream `rvv_vls`
**Old work source:** `feature/rvv-backend` snapshot, January 2026
**Last upstream/research verification:** 2026-09-19

## Purpose

This document set is the implementation contract for building a production-grade, Vector-Length-Agnostic RISC-V Vector backend for simdjson. It is written for a human contributor working with AI coding agents and is deliberately explicit about what is **locked**, what is **tunable**, and what still requires measurement.

The project is not a mechanical port of the January 2026 experimental backend. The old branch is a source of tests, fuzzers, CI/toolchain knowledge, benchmark harnesses, documentation, and selected low-level techniques. The performance core is redesigned against the frozen v4.6.11 source and the upstream `rvv_vls` reference.

## Product statement

The finished product is a new `simdjson::rvv` backend that:

- uses standard RVV 1.0 scalable vectors without requiring fixed `-mrvv-vector-bits=<N>` code generation for the VLA core;
- preserves `rvv_vls` as the fixed-VLEN reference/alternative;
- implements a custom RVV-native Stage 1 rather than emulating `simd8x64` as the hot-path architecture;
- preserves predicates/vector state long enough to test native compaction/index-generation strategies;
- provides complete UTF-8 validation, correct JSON minification, generic Stage 2 integration, DOM and On-Demand behavior;
- supports safe Linux runtime selection in supported build modes;
- is correctness-tested at VLEN 128/256/512/1024 under QEMU;
- is benchmarked against `rvv_vls` on real RVV hardware;
- leaves non-RISC-V behavior unchanged.

## The three contract layers

Do not confuse these layers:

1. **Semantic invariants** — must always hold. See `20_ALGORITHM_INVARIANTS.md`.
2. **Locked engineering decisions** — stable integration/product constraints. See `03_LOCKED_ARCHITECTURE.md` and `14_DECISION_LOG.md`.
3. **Performance hypotheses** — candidate implementation choices that must earn promotion through real measurements. See `21_PERFORMANCE_HYPOTHESES.md`.

An AI agent may implement a hypothesis for testing. It may not silently promote it to a locked decision.

## Read order

1. `01_PRODUCT_REQUIREMENTS.md`
2. `02_BASELINE_AND_UPSTREAM_CONTEXT.md`
3. `20_ALGORITHM_INVARIANTS.md`
4. `03_LOCKED_ARCHITECTURE.md`
5. `21_PERFORMANCE_HYPOTHESES.md`
6. `04_STAGE1_KERNEL.md`
7. `05_UTF8_MINIFY_STAGE2.md`
8. `06_RUNTIME_DISPATCH_BUILD_AMALGAMATION.md`
9. `07_SOURCE_LAYOUT_AND_MIGRATION.md`
10. `08_CORRECTNESS_TESTING_FUZZING.md`
11. `09_PERFORMANCE_BENCHMARKING.md`
12. `23_BENCHMARK_EXPERIMENT_MATRIX.md`
13. `10_CI_TOOLCHAINS_HARDWARE.md`
14. `22_REBASE_POLICY.md`
15. `11_AI_CODING_PROTOCOL.md`
16. `12_IMPLEMENTATION_WORKPACKAGES.md`
17. `13_ACCEPTANCE_RELEASE_UPSTREAM.md`
18. `14_DECISION_LOG.md`
19. `15_RISK_REGISTER.md`
20. `16_TRACEABILITY_MATRIX.md`
21. `24_PROJECT_STATE.md`

## Anti-drift rule

No coding agent may silently change an invariant, a locked decision, the frozen baseline, or an accepted benchmark conclusion. A conflict must be surfaced before implementation continues.

Performance-specific choices are different: they are intentionally provisional until benchmark evidence exists. Agents must label them by hypothesis ID instead of presenting them as architectural truth.

## Canonical repositories during development

- `simdjson-rvv-work`: read-only reference/mine of reusable assets.
- `simdjson-upstream`: target product tree starting from v4.6.11.
- Working branch: `feature/rvv-vla`.

## Source-of-truth hierarchy

When sources disagree, use this order:

1. frozen v4.6.11 source code and its tests for exact internal/public behavior;
2. product requirements and semantic invariants in this spec;
3. locked engineering decisions in this spec;
4. accepted benchmark conclusions recorded in the decision log;
5. active performance hypotheses;
6. upstream `rvv_vls` as implementation/reference precedent;
7. old RVV branch code;
8. AI-generated suggestions.

If v4.6.11 behavior exposes a mistake in this spec, stop, document the conflict, and update the spec. Do not force the source into an incorrect interpretation.

## Machine-readable preflight

Before an AI coding session, read `SPEC_MANIFEST.json`. It mirrors the spec version, mandatory documents, work-package dependencies, locked decisions, active hypotheses, and stop conditions. The Markdown remains authoritative if the JSON and Markdown ever disagree.

# Specification Changelog

## 1.1 — 2026-09-19

Optimization/anti-drift hardening before core coding:

- separated semantic invariants, locked engineering decisions and performance hypotheses;
- removed premature lock to Stage 1 `e8m2`/LMUL=m2;
- changed structural writer from unconditional `vcompress` architecture to measured W1/W2/W3 candidates;
- changed escape design from mandatory dense `viota/gather` path to no-backslash + rare-event/dense/adaptive candidates;
- changed quote-prefix `viota` from requirement to candidate;
- changed fused UTF-8 from lock to U1/U2/U3 scheduling experiment;
- kept native RVV compaction as a required candidate, not an unmeasured guarantee;
- added GCC target-attribute fact/probe requirement and conservative separate-TU build baseline;
- added controlled upstream rebase checkpoints;
- strengthened real-hardware experiment/statistics protocol;
- added machine-readable `SPEC_MANIFEST.json`;
- added `20_ALGORITHM_INVARIANTS.md`, `21_PERFORMANCE_HYPOTHESES.md`, `22_REBASE_POLICY.md`, `23_BENCHMARK_EXPERIMENT_MATRIX.md`, and `24_PROJECT_STATE.md`;
- updated AI session protocol to track hypotheses/evidence explicitly.

## 1.0 — 2026-09-19

Initial complete anti-drift specification. It established v4.6.11 as the frozen target, preserved `rvv_vls`, replaced the January fixed-64-byte/LMUL=m1 architecture with a VLA design, required complete UTF-8/correct minify/runtime dispatch, and added correctness/performance/AI-development protocols.

Version 1.1 supersedes 1.0 where algorithm choices conflict.

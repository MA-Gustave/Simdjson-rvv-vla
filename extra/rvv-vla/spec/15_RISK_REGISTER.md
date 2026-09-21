# Risk Register

## R-01 — no single LMUL wins across VLEN/microarchitectures

**Risk:** m1/m2 register pressure, loop overhead and index-conversion costs differ by CPU/VLEN.  
**Mitigation:** keep candidate shape abstracted through WP-13, inspect assembly, allow evidence-backed adaptive/build-time tuning if necessary.

## R-02 — native structural compaction is not always fastest

**Risk:** `vid`/`vcompress`/widen setup may lose on sparse masks or cores with slow compress.  
**Mitigation:** preserve W2 sparse/reference candidates, sweep structural density, measure crossover including branch cost.

## R-03 — dense escape algorithm costs more than rare-event handling

**Risk:** elegant `viota`/gather/run logic is expensive for ordinary JSON.  
**Mitigation:** mandatory no-backslash path, rare-event E1 candidate, only use dense E2 when density/benchmarks justify it.

## R-04 — quote-prefix primitive is microarchitecture-sensitive

**Risk:** `viota` latency/throughput varies.  
**Mitigation:** treat Q1 as hypothesis; inspect counters/assembly; allow equivalent scalable prefix alternative.

## R-05 — UTF-8 fusion causes spills

**Risk:** fused validation increases live vectors/masks enough to reduce Stage 1 speed.  
**Mitigation:** compare U1/U2/U3 end-to-end, scope temporaries, inspect assembly, do not fetishize one memory pass.

## R-06 — GCC/Clang RVV intrinsic/codegen differences

**Risk:** signatures/optimizations/target attributes differ or contain bugs.  
**Mitigation:** pinned compile probes, small compatibility wrappers only where justified, separate-TU baseline.

## R-07 — runtime capability headers vary

**Risk:** old libc/kernel headers lack hwprobe/HWCAP definitions.  
**Mitigation:** guarded detection and safe fallback; no mandatory new system headers for unrelated builds.

## R-08 — single-header mixed-ISA limitation

**Risk:** function attributes are insufficient for a generic one-TU build on one compiler.  
**Mitigation:** advertise only tested support; normal library can use separate object mode.

## R-09 — Stage 2 limits end-to-end gain

**Risk:** major Stage 1 speedup yields small parser gain.  
**Mitigation:** publish both layers; profile before any Stage 2 expansion.

## R-10 — VLA loses to VLS on some small-VLEN/fixed deployments

**Risk:** compile-time fixed width enables better scheduling.  
**Mitigation:** do not promise universal replacement; position VLA on measured speed plus scalable deployment.

## R-11 — old branch defects contaminate new core

**Risk:** bring-up code contains correctness/performance shortcuts.  
**Mitigation:** mine tests/tooling selectively; rewrite hot core from invariants/current source.

## R-12 — rented hardware noise

**Risk:** noisy neighbors/frequency variation hides 5-15% effects.  
**Mitigation:** dedicated instance where possible, pinning, randomized A/B order, 20+ samples for close results, variance/counter metadata.

## R-13 — AI invents plausible intrinsics or semantics

**Risk:** late compile failures or subtle bugs.  
**Mitigation:** manifest/preflight, intrinsic probes, exact-source semantics, human review.

## R-14 — upstream lands a VLA backend during development

**Risk:** duplicate obsolete work.  
**Mitigation:** mandatory rebase/upstream-monitor checkpoints before hardware tuning and PR preparation.

## R-15 — provenance/copyright issue

**Risk:** close adaptation from external code without license tracking.  
**Mitigation:** source/commit/license records for every adaptation; prefer independent implementation when practical.

## R-16 — adaptive fast paths overfit primary CPU

**Risk:** density thresholds win on one core and regress others.  
**Mitigation:** thresholds remain evidence-scoped; use second hardware before claiming portability; prefer simple policy when gains are within noise.

## R-17 — rebase changes generic semantics/codegen

**Risk:** late upstream changes invalidate assumptions or benchmarks.  
**Mitigation:** checkpoint rebases separated from algorithm commits; rerun differential tests and affected performance experiments.

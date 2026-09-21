# Upstream Rebase Policy

The frozen v4.6.11 baseline isolates algorithm development from upstream churn, but the project must not arrive at submission with months of hidden divergence.

## Checkpoint R0 — start

Record v4.6.11 tag/commit, clean status and baseline test result. No rebase.

## Checkpoint R1 — after complete/correct Stage 1, before hardware tuning

Search upstream for new RVV/VLA work and compare relevant generic Stage 1/build changes. Rebase only if:

- upstream has changed contracts we would otherwise have to re-port later;
- a competing VLA implementation materially changes the project;
- or build/toolchain fixes are necessary for the hardware campaign.

If no compelling reason exists, keep the frozen branch for controlled benchmarking and record the review result.

## Checkpoint R2 — before final hardware benchmark publication

Re-check current upstream. If rebasing changes hot code, compiler flags, generic Stage 2 or `rvv_vls`, rerun both baseline and VLA benchmarks after the rebase. Never compare pre-rebase VLS numbers to post-rebase VLA numbers.

## Checkpoint R3 — immediately before upstream PR preparation

Rebase/port onto the maintainer-requested current branch. Keep rebase/integration commits separate from algorithm-tuning commits where practical. Rerun full correctness and affected performance gates.

## Conflict discipline

For every rebase:

1. capture old base/new base;
2. list upstream files/contracts changed;
3. resolve semantics using upstream tests/source;
4. do not opportunistically retune algorithms in the same commit;
5. run differential tests;
6. rerun benchmarks if generated hot code changed;
7. update `24_PROJECT_STATE.md` and `SPEC_MANIFEST.json` baseline metadata if the project formally moves to the new base.

## Abort/rethink condition

If upstream lands a complete VLA backend, stop the rebase and perform a design/performance comparison before continuing. The project may pivot to a targeted optimization instead of duplicating functionality.

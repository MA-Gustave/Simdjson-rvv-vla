# Project State Ledger

This file is mutable project state, not an architecture specification. Update it after accepted work so a new human/AI session can resume without reconstructing history.

## Current state

```text
SPEC_VERSION: 1.1
BASELINE: simdjson v4.6.11
WORKING_BRANCH: feature/rvv-vla (to be created/confirmed)
CURRENT_WP: WP-00
STATUS: NOT_STARTED
LAST_ACCEPTED_COMMIT: none
LAST_UPSTREAM_CHECK: 2026-09-19
HARDWARE_BENCHMARK_EVIDENCE: none
```

## Accepted performance decisions

None yet. All entries in `21_PERFORMANCE_HYPOTHESES.md` remain hypotheses.

## Test evidence

None yet for the new v1.1 implementation. Baseline results should be recorded during WP-00.

## Open technical questions

1. Exact GCC version and target-region spelling/behavior with the RVV intrinsics used by this backend.
2. Clang mixed-ISA region support; do not assume parity with GCC.
3. Primary real-hardware platform/VLEN for WP-13.
4. Whether E2 dense escape path is necessary after E1/no-backslash performance is known.
5. Whether W2 sparse writer has a meaningful crossover against W1.
6. Whether U1 fusion causes spills relative to U2/U3 on primary hardware.

## Session log format

Append concise entries:

```text
DATE:
COMMIT:
WP:
FILES:
INVARIANTS/DECISIONS/HYPOTHESES:
TESTS:
RESULT:
NEW_EVIDENCE:
OPEN_QUESTIONS:
NEXT_ACTION:
```

Do not record invented/pending test outcomes as passed.

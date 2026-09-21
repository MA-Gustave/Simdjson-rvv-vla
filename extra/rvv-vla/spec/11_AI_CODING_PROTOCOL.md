# AI Coding Protocol

This project is intentionally AI-assisted. The human contributor remains the author and reviewer. Upstream's `AI_USAGE_POLICY.md` explicitly requires human-in-the-loop review and accountability.

## 1. Mandatory preflight

Every coding session starts with:

```text
SPEC_VERSION: 1.1
BASELINE_COMMIT_OR_TAG: ...
WORK_PACKAGE: WP-xx
FILES_ALLOWED_TO_CHANGE: ...
LOCKED_DECISIONS: D-...
INVARIANTS: I-...
PERFORMANCE_HYPOTHESES_UNDER_TEST: H-... or NONE
REQUIRED_TESTS: ...
```

Read `SPEC_MANIFEST.json`, `00_START_HERE.md`, relevant work-package docs, `20_ALGORITHM_INVARIANTS.md`, `03_LOCKED_ARCHITECTURE.md`, and `21_PERFORMANCE_HYPOTHESES.md` before code generation.

## 2. Agent prohibitions

An agent may not:

- invent benchmark/test results;
- claim success without execution output;
- silently promote a hypothesis to a locked decision;
- copy the old RVV core wholesale;
- weaken tests to make code pass;
- use QEMU speed as evidence;
- expose sizeless RVV types through ABI/public state;
- add fixed-VLEN requirements to the VLA Stage 1 core;
- modify unrelated generic code for convenience;
- guess RVV intrinsic spellings and stack further code on the guess;
- change frozen/rebased baseline without the rebase protocol.

## 3. Required patch report

Every coding patch reports:

1. intent;
2. exact files changed;
3. invariants preserved;
4. decision IDs touched;
5. hypothesis IDs implemented/compared;
6. algorithm note for tricky state;
7. risks/open compiler questions;
8. exact tests/commands;
9. actual test output status;
10. explicit statement that performance is unproven unless real-hardware output is attached.

## 4. Patch boundaries

The final product scope is complete, but code should be reviewable. Use work-package boundaries or smaller coherent patches. "No baby steps" means do not shrink the product vision; it does not justify an unreviewable monolithic diff.

## 5. Human review checklist

The human must be able to explain the invariant, RVV type/LMUL legality, behavior at `vl=1`, vector boundaries, empty quote/backslash masks, trailing backslash runs, memory bounds, VLEN variation, non-RVV compilation, and the tests that prove the difficult path.

## 6. Intrinsic anti-hallucination rule

When an intrinsic/signature is uncertain:

1. inspect installed `<riscv_vector.h>` or compiler documentation;
2. compile a minimal probe;
3. record the result;
4. only then use it in the backend.

## 7. Simdjson semantic uncertainty

Read the exact frozen/rebased source and tests. Do not replace internal behavior with generic JSON intuition.

## 8. Hypothesis workflow

```text
hypothesis -> candidate code -> correctness -> real benchmark -> decision
```

Until that final decision exists, wording must remain "candidate", "hypothesis" or "experimental".

## 9. Provenance

For code/algorithms adapted from simdutf or elsewhere, identify source/commit, verify compatible licensing, retain required attribution, and document the adaptation. AI regeneration does not erase copyright obligations.

## 10. Source style

Follow upstream ASCII/style conventions, minimal formatting churn, English comments, no core output/abort/exit, and `SIMDJSON_` prefix for public macros.

## 11. Stop conditions

Stop and request a human/spec decision if:

- frozen/rebased source contradicts the spec;
- an invariant cannot be maintained;
- a required intrinsic/build mechanism is unavailable;
- implementation requires an unplanned public API;
- a nontrivial generic-file change is needed;
- a hypothesis needs promotion to become a dependency of later work;
- upstream has introduced a competing VLA implementation that materially changes the plan.

## 12. Session handoff

Update `24_PROJECT_STATE.md` after accepted work with commit, WP status, test evidence, active algorithm choices, unresolved questions and next action. This prevents cross-session AI drift.

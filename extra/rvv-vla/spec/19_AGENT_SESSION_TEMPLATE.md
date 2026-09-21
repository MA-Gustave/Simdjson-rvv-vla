# AI Agent Session Template

## Session identity

```text
SPEC_VERSION: 1.1
BASELINE_TAG_OR_COMMIT: v4.6.11 / <rebased commit if approved>
BRANCH: feature/rvv-vla
WORK_PACKAGE: WP-__
```

## Read before coding

```text
SPEC_MANIFEST.json
00_START_HERE.md
20_ALGORITHM_INVARIANTS.md
03_LOCKED_ARCHITECTURE.md
21_PERFORMANCE_HYPOTHESES.md
<work-package-specific docs>
14_DECISION_LOG.md
24_PROJECT_STATE.md
```

## Allowed files

```text
<exact paths>
```

## Contract touched

```text
INVARIANTS: I-___
LOCKED_DECISIONS: D-___
HYPOTHESES_UNDER_TEST: H-___ or NONE
```

## Required behavior/tests

```text
<acceptance behavior>
<exact commands/targets>
```

## Required agent output

1. design/intent summary;
2. exact changed files;
3. invariant/decision/hypothesis mapping;
4. tricky boundary behavior;
5. exact validation commands;
6. actual execution results or explicit "not run";
7. compiler/intrinsic uncertainties;
8. no performance claim without hardware data;
9. proposed `24_PROJECT_STATE.md` update.

## Stop conditions

Stop if source contradicts the spec, an invariant cannot be met, an intrinsic/build mode is unavailable, a new public API becomes necessary, a locked decision must change, a hypothesis must be promoted without evidence, generic code needs an unplanned invasive change, or upstream lands a materially competing VLA implementation.

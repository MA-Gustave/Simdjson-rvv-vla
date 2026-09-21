# Implementation Work Packages

All packages belong to the full product unless explicitly marked exploratory. Performance candidates may coexist until the hardware-selection gate.

## WP-00 — Freeze, probes and evidence capture

- confirm v4.6.11 target commit/tag and clean baseline tests;
- create `feature/rvv-vla`;
- run/compiler-document RVV intrinsic probes;
- prove separate-TU linkage mode;
- probe GCC target-region mode and Clang mode independently;
- initialize `24_PROJECT_STATE.md`.

## WP-01 — Backend skeleton and compile isolation

- activate reserved `rvv` identity after verifying it is free;
- create current-style headers/source;
- register/force-select backend;
- isolate RVV compilation safely;
- keep non-RISC-V builds unaffected.

## WP-02 — Runtime RVV detection

- guarded Linux hwprobe/HWCAP detection;
- supported-by-runtime behavior;
- compile-time always-RVV mode;
- safe fallback on old headers/platforms;
- no SIGILL probing.

## WP-03 — VLA string/escape engine with candidate paths

- minimal scalar carry state;
- mandatory no-backslash fast path;
- implement E1 rare-event path;
- implement E2 dense-vector path only if feasible/needed for comparison;
- scalable quote-parity candidate Q1 and any justified alternative;
- exhaustive boundary/differential microtests.

Do not prematurely delete losing candidates before real-hardware evidence if they are cheap to retain in benchmark-only form.

## WP-04 — VLA classifier and pseudo-structurals

- whitespace/operator/control classification;
- scalar-start semantics;
- cross-chunk previous-scalar state;
- control-character errors;
- structural predicate;
- differential predicate/index tests.

## WP-05 — Structural writers and density policy

- W1 native vector-compaction writer for at least the principal m1/m2 shapes;
- W2 sparse/event writer where practical;
- W3 reference/ablation writer;
- exact index parity tests;
- synthetic density benchmark harness;
- no permanent threshold before hardware data.

## WP-06 — UTF-8 candidates

- complete standalone RVV validator;
- parser schedule U1/U2 and U3 reference as needed;
- boundary/tail behavior;
- Unicode differential tests/fuzzing;
- preserve candidate separation until end-to-end hardware measurement.

## WP-07 — Full Stage 1 integration and streaming

- capacity/empty handling;
- regular/partial/final semantics;
- sentinel/count bookkeeping;
- unclosed strings;
- UTF-8/error integration;
- DOM Stage 1 entry point;
- frozen-source differential tests.

## WP-08 — Stage 2 adapters

- VLA fixed-semantic-window string helpers;
- generic string/number/tape-builder integration;
- parse/wobbly parity;
- no unnecessary Stage 2 optimization.

## WP-09 — Correct minify

- shared/equivalent string-state logic;
- outside-string whitespace;
- M1 compaction and reference/alternate path if useful;
- exact differential tests/fuzzer.

## WP-10 — DOM, On-Demand, builder/public integration

- full relevant DOM/On-Demand tests;
- umbrella headers and forcing/public includes;
- no API expansion.

## WP-11 — Amalgamation/single-header

- update generator/list;
- test each claimed mode;
- document unsupported mixed-ISA modes honestly.

## WP-12 — CI/tooling migration

- consolidated QEMU scripts/toolchains;
- GCC/Clang VLEN matrix;
- smoke/full/nightly tiers;
- capability/probe tools;
- migrate useful old tests/fuzz/bench assets and remove stale duplicates.

## WP-13 — Real-hardware algorithm selection and tuning

This is the promotion gate for performance hypotheses:

- measure `rvv_vls` baseline;
- compare m1/m2 and any justified vector-shape candidate;
- compare structural writers and adaptive crossover;
- compare escape paths/thresholds where relevant;
- compare UTF-8 schedules end-to-end;
- inspect generated assembly/spills;
- record chosen defaults as ADRs;
- rerun complete benchmark corpus after each promoted choice.

## WP-14 — Rebase checkpoint, hardening and reproduction

- execute mandatory rebase policy checkpoint;
- rerun full correctness/fuzz matrix;
- warning/style cleanup caused by backend;
- synchronize docs/manifest/state;
- preserve benchmark reproduction scripts/raw data.

## WP-15 — Upstream package

- focused commit series;
- correctness matrix;
- hardware/performance report with raw artifacts;
- VLA/VLS design explanation;
- AI-use disclosure/process explanation;
- no unrelated changes.

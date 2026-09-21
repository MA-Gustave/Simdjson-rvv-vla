# Architecture Decision Log

This file contains accepted decisions only. Performance hypotheses remain in `21_PERFORMANCE_HYPOTHESES.md` until measured and promoted.

| ID | Status | Decision | Rationale |
|---|---|---|---|
| D-001 | Locked | backend key `rvv` | reserved/public terminology |
| D-002 | Locked | namespace `simdjson::rvv` | implementation consistency |
| D-003 | Locked | preserve `rvv_vls` | fixed-VLEN reference/alternative |
| D-004 | Locked | frozen development baseline v4.6.11 with controlled rebase checkpoints | isolate algorithm and integration changes |
| D-010 | Locked | no fixed-vector-bit dependency in VLA Stage 1 core | scalable product definition |
| D-011 | Locked | sizeless vectors stay implementation-local | ABI/type constraints |
| D-012 | Locked | RVV 1.0 baseline, optional extensions guarded | portability |
| D-020 | Locked | custom VLA Stage 1 hot loop | avoid fixed-width abstraction tax |
| D-021 | Locked | LMUL/vector shape is evidence-selected, not pre-locked | microarchitecture dependence |
| D-022 | Locked | no universal 64-byte Stage 1 correctness block | VLA semantics |
| D-023 | Locked | predicates remain first-class; scalar event extraction allowed | avoid forced u64-mask architecture |
| D-030 | Locked | vector-compaction and reference writer retained through selection gate | measure core hypothesis |
| D-040 | Locked | minimal scalar cross-chunk state | scalable state machine |
| D-041 | Locked | no-backslash fast path | common JSON fast path |
| D-042 | Locked | scalable/density-aware escape handling; no per-byte scalar production loop | correctness + performance flexibility |
| D-043 | Locked | scalable quote parity; exact prefix mechanism is tunable | avoid premature viota lock |
| D-050 | Locked | complete UTF-8 | correctness |
| D-051 | Locked | UTF-8 scheduling selected by end-to-end evidence | register/memory trade-off |
| D-060 | Locked | string-aware minify | correctness |
| D-061 | Locked | native RVV minify compaction candidate required; exact store policy tunable | performance evidence |
| D-070 | Locked | reuse generic Stage 2 initially | focus on measured hotspot |
| D-071 | Locked | fixed semantic string-helper window allowed, implemented VLA internally | generic interface compatibility |
| D-080 | Locked | OS capability detection; no SIGILL probing | safe dispatch |
| D-081 | Locked | mixed-ISA isolation must be proven; separate TU is conservative baseline | compiler/toolchain safety |
| D-082 | Locked | single-header support is claimed only when tested | avoid false parity |
| D-083 | Locked | VLA/VLS automatic priority requires evidence | no universal performance assumption |
| D-090 | Locked | minimal switchboard changes | maintainability |
| D-091 | Locked | no unrelated refactors | reviewability |
| D-092 | Locked | performance claims require real hardware | scientific validity |
| D-093 | Locked | QEMU correctness-only | invalid speed model |
| D-094 | Locked | human review/understanding of AI code | upstream policy/accountability |
| D-095 | Locked | hypotheses cannot be silently promoted | anti-drift |

## Performance decisions

When WP-13 promotes a hypothesis, append a row such as:

```text
P-001 | Accepted | H-LMUL-01 selects m2 on CPU family X | evidence artifact ...
```

Performance decisions may be scoped to a compiler/CPU class and may later become adaptive policies.

## Changing a locked decision

Require exact ID, conflict/evidence, alternative, correctness impact, benchmark evidence when performance-related, affected-doc updates, and human acceptance.

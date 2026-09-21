# Performance Hypotheses

This file is intentionally provisional. Hypotheses may be rejected without changing product scope. Promotion requires the process in `09_PERFORMANCE_BENCHMARKING.md`.

## H-LMUL-01 — Stage 1 byte vector shape

**Question:** which LMUL best balances lanes/iteration, register pressure, mask cost and index conversion?

Mandatory candidates on primary hardware:

- **L1:** e8m1, natural same-lane `u16m2 -> u32m4` writer chain;
- **L2:** e8m2, natural same-lane `u16m4 -> u32m8` writer chain.

Optional research candidate:

- **L4:** e8m4 only if a legal/effective structural writer avoids impossible/awkward widening pressure and register starvation.

Do not assume m2 wins because it processes twice the bytes.

## H-IDX-01 — Structural writer

Candidates:

- **W1 vector:** `vid` -> `vcompress` -> widen/add/store;
- **W2 sparse/event:** extract only a small number of set positions without constructing a wide packed index vector;
- **W3 reference:** simple correctness/ablation writer.

Hypothesis: W1 wins at medium/high density, W2 may win at very low density. An adaptive threshold may outperform either alone.

Required experiment: density sweep plus real corpora, including branch overhead.

## H-ESC-01 — Escape processing

Candidates:

- **E0:** mandatory no-backslash fast path;
- **E1:** rare-event/run path using event positions and bounded scalar work per run/event;
- **E2:** dense vector run-parity using `viota`/run starts/gather/arithmetic or equivalent;
- **EA:** adaptive E1/E2 policy using cheap density/count signal.

Hypothesis: ordinary JSON strongly favors E0/E1; E2 matters only for escape-heavy chunks.

## H-QUOTE-01 — Quote prefix parity

- **Q1:** `viota(unescaped_quote)` plus current-bit parity;
- **Q2:** another scalable prefix-parity construction if compiler/ISA evidence suggests lower cost;
- **QREF:** reference/debug implementation for differential checks.

Hypothesis: Q1 is simple and scalable, but its cost must be measured on the target microarchitecture.

## H-CLASS-01 — Character classification

- direct compares/ORs;
- nibble/table gather scheme analogous to optimized fixed-width backends;
- hybrid classification if one path reduces instructions without extra latency.

Evaluate on ASCII-heavy and structural-dense corpora.

## H-U8-01 — Parser UTF-8 schedule

- **U1 fused same-load:** shortest memory path, highest possible live-register pressure;
- **U2 adjacent/software-pipelined:** reduce live-range conflict while staying in same traversal;
- **U3 separate pass reference:** extra memory pass but simpler/faster kernels.

Hypothesis: U1/U2 should usually win, but U3 remains legitimate if it improves end-to-end throughput on real hardware.

## H-U8-02 — Standalone validator LMUL

Test at least one high-throughput wide LMUL candidate against a lower-pressure candidate. Do not force Stage 1's chosen LMUL onto standalone validation.

## H-MIN-01 — Minify store

- **M1:** predicate + `vcompress` + contiguous store;
- **M2:** keep-run/event copy strategy if compress throughput is poor.

Expected winner is M1, but measure before locking.

## H-DISP-01 — Backend preference

In a deployment where both `rvv` and `rvv_vls` are actually selectable, prefer the backend supported by representative hardware evidence. Do not use naming/newness as priority.

## H-ZVBB-01 — Optional Zvbb

After baseline V correctness/performance is stable, evaluate guarded Zvbb-specific fast paths only if they improve a measured hotspot. Baseline correctness must never depend on Zvbb.

## Promotion record format

For each promoted hypothesis, record:

```text
Hypothesis ID:
Candidates:
Hardware / VLEN:
Compiler / flags:
Corpus / experiment ID:
Median / geometric-mean result:
Counters / assembly observations:
Rejected regressions:
Decision scope:
Linked raw artifact:
```

# Traceability Matrix

| Requirement / invariant | Primary code | Primary tests | Performance evidence |
|---|---|---|---|
| backend `rvv` identity | detection/implementation/source | compile/selection tests | N/A |
| VLA core without fixed bits | `rvv` Stage 1/build flags | VLEN matrix | build metadata |
| safe runtime V detection | isadetection/portability | capability/force tests | N/A |
| escape semantics | string-state kernel | exhaustive boundary + fuzz | E1/E2 density sweep |
| scalable quote parity | string-state kernel | boundary structural tests | Q candidate comparison if needed |
| pseudo-structurals | Stage 1 | exact index differential | corpus Stage 1 |
| structural writer correctness | writer candidates | exact index parity | W1/W2/W3 density sweep |
| complete UTF-8 | UTF-8 helper | Unicode corpus/fuzz | standalone throughput |
| parser UTF-8 scheduling | Stage 1 + UTF-8 helper | parser Unicode tests | U1/U2/U3 end-to-end |
| correct minify | minify | byte-exact + fuzz | M candidate throughput |
| generic Stage 2 | source + defs | DOM/On-Demand/string/number | end-to-end |
| non-RISC-V isolation | conditional build/include | host CI | N/A |
| single-header factual status | amalgamation | claimed-mode compile/run | N/A |
| GCC/Clang supported modes | toolchains/probes | full matrix | compiler metadata |
| VLEN 128/256/512/1024 | QEMU tooling | RVV tests | no speed claim |
| real-hardware proof | benchmark scripts | pre-bench smoke | raw CSV/counters/metadata |
| hypothesis promotion discipline | decision/state docs | review checklist | linked experiment IDs |
| AI human-in-loop | process docs | human checklist | N/A |
| controlled rebase | rebase log/state | full regression | rerun affected benches |

## Work-package map

```text
Freeze/probes          -> WP-00
Identity/build         -> WP-01, WP-02
String state           -> WP-03
Stage 1 semantics      -> WP-04
Index writers          -> WP-05
UTF-8                  -> WP-06
Streaming/finalization -> WP-07
Stage 2                -> WP-08
Minify                 -> WP-09
Public integration     -> WP-10
Amalgamation           -> WP-11
CI/tooling             -> WP-12
Hardware selection     -> WP-13
Rebase/hardening       -> WP-14
Upstream submission    -> WP-15
```

Every accepted patch identifies rows and decision/invariant/hypothesis IDs it touches.

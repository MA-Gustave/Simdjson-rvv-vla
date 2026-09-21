# Evidence and Constraints

## 1. Evidence hierarchy

Performance decisions should use the following evidence order:

1. **Native end-to-end simdjson measurements on the target RVV machine.**
2. Native Stage 1 / minify / UTF-8 measurements on the same machine.
3. Native microbenchmarks for exact instruction sequences.
4. Assembly inspection and performance counters.
5. Published measurements from the same microarchitecture.
6. Published measurements from a different RVV microarchitecture.
7. Compiler-generated instruction count.
8. QEMU timing — **not valid for performance conclusions**.

QEMU is still valuable for correctness and for rapidly validating algorithm variants across
VLEN 128/256/512/1024.

## 2. The most relevant external work

### simdjson RVV-VLS

The upstream `rvv_vls` backend was added by Olaf Bernstein (`camel-cdr`) in simdjson
PR #2593. The PR explicitly states that a future fully scalable VLA `rvv` backend with
proper runtime dispatch would require a substantially new implementation rather than just
the fixed-width generic backend.

This makes `rvv_vls` both our baseline and an important architectural reference.

### rvv-bench

`camel-cdr/rvv-bench` measures:

- instruction throughput;
- different LMUL choices;
- gathers, compresses, slides and reductions;
- real algorithms;
- multiple real and simulated RVV implementations.

The result database includes the SpacemiT X60 / K1, which is especially useful because it
is a realistic first target for our own native measurements.

### RVV integer-workload gap analysis

Olaf's RVV gap analysis calls out several weaknesses that directly overlap our Stage 1
problem:

- mask slides are awkward in portable VLA code;
- using element slides as a substitute for mask slides becomes more expensive with LMUL;
- scan/prefix operations on masks are useful and under-supported;
- a hypothetical mask XOR scan (`vmsxff.m`) would directly accelerate prefix parity.

These observations are highly relevant to quote parity, escaped-character propagation and
scalar-start detection.

## 3. X60 evidence that matters to us

The SpacemiT X60 benchmark page reports VLEN=256 and measures instruction throughput
under repeated/unrolled execution.

For `e8` vectors, the published measurements show an especially strong LMUL sensitivity
for permutation instructions:

| operation | m1 | m2 | m4 | m8 |
|---|---:|---:|---:|---:|
| `vcompress.vm` | ~3 | ~10 | ~36 | ~136 |
| `vrgather.vv` | ~4 | ~16 | ~64 | ~256 |
| `viota.m` | ~2 | ~4 | ~8 | ~16 |
| `vslide1up.vx` | ~2 | ~4 | ~8 | ~16 |

By contrast, mask reductions such as `vfirst.m` / `vcpop.m` are reported around two
cycles in the same table, and mask stores (`vsm.v`) are relatively inexpensive.

These figures are **microbenchmark throughput estimates**, not direct costs for our parser.
They should guide hypotheses, not substitute for parser benchmarks.

The immediate conclusion is not "never use `vcompress` or `vrgather`". It is:

> Do not scale a permutation-heavy algorithm to high LMUL without evidence that the
> extra lanes amortize the rapidly rising permutation cost.

## 4. Baseline ISA policy

The portable backend should remain correct and performant with baseline RVV 1.0.

Optional extensions can be explored only as guarded variants. Examples:

- Zvbb;
- Zbkb/Zbkx scalar `xperm4` experiments;
- future mask-slide or scan extensions if they become available.

No optional extension should be required for baseline correctness.

## 5. Compiler policy

GCC and Clang can produce materially different RVV instruction schedules.

For any promoted optimization:

- inspect both GCC and Clang assembly;
- record compiler version;
- record complete flags;
- keep compiler family fixed in a given speedup comparison;
- inspect unexpected `vsetvli` churn;
- verify that helper abstractions did not introduce repeated vtype changes or spills.

## 6. Hardware policy

The first optimization cycle may be X60-driven because we can measure it.

However, promotion into the portable default should require confirmation on at least one
additional RVV implementation when practical. Candidate secondary targets include newer
SpacemiT K3 cores and other RVV 1.0 systems represented in the public `rvv-bench-results`
database.

An X60-specific win that regresses substantially elsewhere should become either:

- a runtime/microarchitecture-specific path, if maintainable; or
- a rejected optimization.

## 7. Performance objective

The objective is not a fixed percentage. The objective is:

- no correctness regression;
- no loss of VLA semantics;
- no dependence on a fixed compile-time VLEN;
- measurable benefit in representative parser workloads;
- no severe regression on important input classes;
- maintainable code and explainable performance behavior.

A Stage 1 microbenchmark improvement is useful only if it survives into full parse or
materially benefits standalone Stage 1 users.

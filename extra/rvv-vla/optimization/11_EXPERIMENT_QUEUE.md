# Concrete Experiment Queue

This is the recommended execution order after obtaining native RVV hardware.


## Current implementation checkpoint

The first code wave now contains unpromoted candidates for EXP-101 through EXP-104:

- EXP-101: sparse structural event writer (`count <= 8`) with dense W1 fallback;
- EXP-102: packed-mask 64-bit quote prefix XOR for up to 512 active lanes;
- EXP-103: one/two-event sparse escape path, packed 64-bit escape algebra for up to 512 lanes, and dense vector fallback;
- EXP-104: packed one-bit scalar-start shift with vector fallback.

These are **implementation candidates, not measured winners**. Native baseline and A/B evidence remain required before promotion. See `12_FIRST_WAVE_IMPLEMENTED.md`.

## Phase 0 — baseline

### EXP-000 Native baseline

Run the existing harness unchanged.

Record:

```text
rvv current
rvv_vls native VLEN
fallback
Stage 1
DOM
Stage 2
minify
UTF-8
```

Do not optimize before this result is archived.

## Phase 1 — prove the hotspot model

### EXP-101 Structural writer density sweep

Compare:

```text
W0 current u32m8 compress
W1 packed mask + ctz
W2 unrolled ctz
```

Decision:
- if scalar/event emission wins on real JSON, make it the default candidate;
- if crossover exists, retain both and determine a threshold.

### EXP-102 Quote prefix

Compare:

```text
Q0 current viota-based parity
Q1 packed 64-bit prefix XOR
```

Keep byte classification unchanged.

### EXP-103 Escape runs

Compare:

```text
E0 current vector run path
E1 packed-mask scalar run/event path
E2 adaptive E1/E0
```

Sweep event density and run length.

### EXP-104 Scalar-start shift

Compare:

```text
S0 current byte flags + vslide1up
S1 packed mask << 1 + carry
```

## Phase 2 — compose control-plane changes

### EXP-201 e8m2 bitset Stage 1

Combine only proven Phase-1 winners while retaining e8m2.

This is the most important architecture checkpoint.

If this does not beat the current Stage 1, investigate composition overhead before widening
the vector.

## Phase 3 — width / LMUL

### EXP-301 e8m1 unrolled

Test a lower-LMUL version with the new control plane.

### EXP-302 e8m4 wide input

Test e8m4 only after high-LMUL structural/string permutations have been removed.

Compare:

```text
e8m1 unrolled
e8m2
e8m4
```

on multiple corpora.

### EXP-303 runtime width policy

Only if winners differ materially by VLEN or hardware, test a runtime VLEN-based choice.

## Phase 4 — classification

### EXP-401 current split-m1 LUT

Baseline.

### EXP-402 direct compares

Compare whitespace/operators with direct comparisons and ORs.

### EXP-403 explicit m1 sub-kernels

Keep LUT method but express larger logical byte groups as repeated m1 chunks to minimize
vtype/gather overhead.

Inspect assembly carefully.

## Phase 5 — UTF-8

### EXP-501 fused current

Baseline.

### EXP-502 explicit m1 lookup4 scheduling

Reduce helper/vtype churn.

### EXP-503 separate validation pass

Measure register-pressure trade-off.

### EXP-504 standalone LMUL sweep

Independent of Stage 1.

### EXP-505 optional xperm4 path

Only on hardware with the required scalar extension.

## Phase 6 — minify

### EXP-601 bitset string state

Keep current output compression.

### EXP-602 compression LMUL slicing

If m4 compress is measured expensive, classify wide and compress smaller slices.

### EXP-603 sparse whitespace removal

Compare run copying against compress on low deletion density.

## Phase 7 — cross-hardware confirmation

Run final candidate without retuning on a second RVV 1.0 microarchitecture.

If performance changes direction:

1. identify the responsible primitive;
2. compare public rvv-bench instruction data;
3. decide whether one robust algorithm exists;
4. otherwise consider a small hardware-specific policy.

## Immediate coding order after the baseline

If native measurements confirm the current hotspot model, implement in this order:

```text
1. structural packed-mask writer
2. scalar-start bit shift
3. quote prefix XOR
4. sparse escape-run bitset algorithm
5. combined e8m2 control plane
6. e8m4 experiment
```

This ordering minimizes the number of simultaneous unknowns.

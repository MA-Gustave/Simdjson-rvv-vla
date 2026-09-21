# Second Optimization Wave — Packed Structural Writer and Native A/B Harness

## Status

This wave adds a new **unpromoted** structural-index writer candidate and the
native A/B infrastructure needed to measure the first-wave optimizations in
isolation. It must pass the full correctness gate before any performance result
is interpreted.

## Structural writer topology

Stage 1 now has three bounded writer regimes:

```text
W2 sparse mask-event writer
   vfirst + vmsof + vmxor + scalar stores

W1 packed-mask writer
   vsm.v -> uint64_t words -> scalar set-bit enumeration -> scalar u32 stores

W0 dense vector fallback
   vid u32m8 -> vcompress u32m8 -> vector stores
```

Default thresholds are:

```text
SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD=8
SIMDJSON_RVV_PACKED_INDEX_THRESHOLD=64
SIMDJSON_RVV_ENABLE_PACKED_INDEX_WRITER=SIMDJSON_RVV_ENABLE_PACKED_CONTROL
```

The W1 path is bounded by the existing packed-mask capacity (512 active lanes).
If a vector is wider than that window, or the event count exceeds the packed
threshold, Stage 1 falls back to W0. This is therefore a performance policy,
not a VLA correctness limit.

At VLEN=256, Stage 1 uses e8m2 and receives 64 active byte lanes. The whole
structural predicate therefore fits in one uint64_t word, and the default W1
threshold can cover every non-W2 block on the initial SpacemiT K1/X60 target.

The scalar bit enumerator uses Zbb `ctz` when the translation unit enables Zbb.
Otherwise it uses an inlined scalar binary-search sequence, so Zbb is not a
correctness requirement and the existing `-march=rv64gcv` baseline remains
valid.

## Split packed-control switches

`SIMDJSON_RVV_ENABLE_PACKED_CONTROL` remains the umbrella/default switch, but
three internal switches can now override its subpaths independently:

```text
SIMDJSON_RVV_ENABLE_PACKED_ESCAPE
SIMDJSON_RVV_ENABLE_PACKED_QUOTE
SIMDJSON_RVV_ENABLE_PACKED_SHIFT
```

This is intended only for the performance lab. It lets native experiments
attribute a gain or regression to one algorithm without editing source.

## Native A/B variants

`scripts/rvv/perf/bench_native.py` accepts:

```text
--vla-variants phase1
```

which builds:

```text
current_vla
vla_legacy
vla_sparse_index
vla_packed_index
vla_sparse_escape
vla_packed_escape
vla_packed_quote
vla_packed_shift
```

`current_vla` is always retained as the composed candidate. `vla_legacy` is the
A/B control. The generated report contains a separate RVV-VLA A/B section in
addition to the normal current-VLA versus upstream-RVV-VLS comparison.

## Correctness gate for this wave

Run before native benchmarking:

```text
GCC global:    VLEN 128,256,512,1024
GCC dispatch:  VLEN 128,256,512,1024
Clang global:  VLEN 128,256,512,1024
upstream acceptance GCC: all four VLENs
RepoDiag rvv-full
```

The previous 10/10 matrices predate the W1 packed structural writer and must not
be treated as validation of this wave.

## First native commands

Baseline/composed candidate:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile quick --compiler gcc
```

Phase-1 attribution run:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile quick --compiler gcc \
  --vla-variants phase1
```

Do not use QEMU results for performance decisions.

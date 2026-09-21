# First Optimization Wave — Implemented Candidate

## Status

This file records code that has been implemented as an optimization candidate but has **not yet been promoted by native performance evidence**.

The correctness-first vector paths are retained as fallbacks where practical. The next required step is to rerun the complete GCC/Clang correctness matrix and then benchmark on physical RVV hardware.

## Implemented changes

### W2 adaptive structural writer

`src/rvv/stage1.h` now uses a bounded event writer for structural masks containing at most eight events.

The sparse path uses only mask-first operations:

```text
vcpop
vfirst
vmsof
vmxor
scalar uint32 store
```

It avoids constructing `u32m8` lane IDs and avoids `u32m8 vcompress` for sparse blocks. Dense blocks retain the previous W1 vector-compaction path.

The threshold of eight is intentionally a benchmark parameter, not an architectural constant.

### E1 sparse escape path

For one or two backslashes, Stage 1 and minify enumerate only the actual backslash events and synthesize the escaped-byte predicate without `vcompress`, `viota`, or `vrgather`.

The path is restricted to `vl <= 256`, where byte lane IDs are exact. Wider vectors fall through to the packed or fully scalable vector path.

### Packed escape control plane

For non-sparse escape masks with up to 512 active lanes, quote/backslash control is exported with `vsm.v` into packed 64-bit mask words.

Backslash parity then uses the same subtraction/XOR algebra as simdjson's generic 64-byte escape scanner. Carry between words preserves an escape run crossing a 64-bit control-word boundary.

The result is loaded back as an RVV predicate with `vlm.v`.

This removes the high-LMUL chain:

```text
vid -> vcompress -> viota -> vrgather -> arithmetic -> slide
```

from the normal tested VLEN range.

### Q1 packed quote-prefix parity

For quote-containing chunks with up to 512 active lanes, inside-string state now uses packed 64-bit prefix XOR.

For each 64-bit word:

```text
x ^= x << 1
x ^= x << 2
x ^= x << 4
x ^= x << 8
x ^= x << 16
x ^= x << 32
```

Incoming string state is XORed over the valid word. The last valid parity bit becomes the carry into the next word.

The previous `u16m4/u16m8 viota` implementation remains as the fallback for wider vectors and non-little-endian targets.

### S1 packed scalar-start shift

Stage 1's `follows_nonquote_scalar` predicate now uses a packed one-bit shift for the normal optimized range:

```text
shifted = (word << 1) | incoming_bit
```

The outgoing final bit simultaneously replaces the previous `mask_last()` operation.

The former byte expansion + `vslide1up` sequence remains as fallback.

### Boundary coverage

`rvv_stage1_boundary_tests` now exercises backslash runs `1..16` rather than `1..8`, so both the sparse event path and the packed-mask path are exercised around the existing boundary matrix.

## Optimized range and fallback

The packed-control path is currently bounded to 512 active lanes.

This covers the project's existing correctness matrix through VLEN=1024:

```text
Stage 1 e8m2: VLEN=1024 -> 256 lanes
minify e8m4:  VLEN=1024 -> 512 lanes
```

It is **not** a correctness cap. For larger active vectors, the prior fully scalable vector algorithms remain available.

The packed path is enabled only on little-endian targets because it intentionally interprets the packed mask bytes as 64-bit words.

## Compile-time A/B controls

The candidate paths have internal compile-time controls so native experiments can compare algorithms without editing source:

```text
-D SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD=1
-D SIMDJSON_RVV_SPARSE_ESCAPE_THRESHOLD=0
-D SIMDJSON_RVV_ENABLE_PACKED_CONTROL=0
```

The combination above approximates the previous control-plane behavior: the structural writer keeps only its original single-event fast path, the new sparse escape event path is disabled, and packed quote/escape/scalar-shift control is disabled.

Candidate defaults are:

```text
SIMDJSON_RVV_SPARSE_INDEX_THRESHOLD=8
SIMDJSON_RVV_SPARSE_ESCAPE_THRESHOLD=2
SIMDJSON_RVV_ENABLE_PACKED_CONTROL=1
```

These macros are performance-lab controls, not public API commitments.

## Required validation before performance claims

Run, in order:

```text
GCC global:    128,256,512,1024
GCC dispatch:  128,256,512,1024
Clang global:  128,256,512,1024
upstream acceptance
native quick benchmark
native standard benchmark
```

On native hardware, record separately:

- structural writer density crossover;
- Stage 1 throughput;
- quote-heavy JSON;
- escape-heavy JSON;
- ordinary Twitter/CITM corpora;
- minify;
- GCC versus Clang assembly.

## Promotion rule

If the packed path does not produce a repeatable native improvement, revert or retune the relevant sub-path independently. The code should not be kept merely because its instruction sequence looks theoretically better.

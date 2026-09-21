# Structural Index Writer Redesign

## 1. Why this is priority #1

The current writer uses `u32m8` + `vcompress` for every chunk containing more than one
structural event.

On the SpacemiT X60, published microbenchmarks show very steep LMUL scaling for
`vcompress.vm`. This makes the current wide compaction path the clearest candidate for
large performance gains.

The optimization should therefore begin with index emission before changing classification
or UTF-8.

## 2. Required output

Stage 1 must append exact byte offsets of structural characters in increasing order:

```text
base + bit_position
```

No vector representation is required by downstream code. The final product is a scalar
array of `uint32_t`.

This fact gives us freedom to use scalar event extraction.

## 3. Candidate writers

### W0 — current vector writer

Keep as reference:

```text
count == 0 -> return
count == 1 -> vfirst + scalar store
else:
    vid u32m8
    vcompress u32m8
    add base
    store packed indexes
```

### W1 — scalar `ctz` writer from packed mask

Given a 64-bit structural word:

```text
while (bits != 0) {
    i = ctz(bits)
    *tail++ = base + i
    bits &= bits - 1
}
```

Properties:

- work proportional to event count;
- no vector permutation;
- excellent for sparse masks;
- naturally preserves order.

JSON structural density varies. Measure, do not assume.

### W2 — unrolled scalar writer

Reduce branch overhead by extracting several events per loop:

```text
i0 = ctz(bits); clear
i1 = ctz(bits); clear
...
```

with a count or branch threshold.

This may help on in-order cores.

### W3 — adaptive scalar/vector

Use popcount to choose:

```text
0 events        -> return
1..T events     -> scalar ctz
>T events       -> vector writer
```

Threshold `T` must be measured for each candidate vector writer.

### W4 — table-assisted nibble/byte extraction

For very small logical blocks, use a precomputed table of set-bit positions.

This is only worth exploring if branchy `ctz` becomes a measured bottleneck.

## 4. Density experiment

Construct deterministic masks at:

```text
0%
0.5%
1%
2%
5%
10%
20%
40%
70%
100%
```

Measure:

- cycles per 64 input bytes;
- cycles per emitted index;
- branch misses if available;
- instructions;
- crossover point.

Then repeat with real JSON structural distributions.

The synthetic result defines the mechanism; real JSON decides promotion.

## 5. Wider Stage 1 interaction

A bitset writer removes the requirement that byte lanes and `uint32_t` output lanes match.

This is what enables testing:

```text
e8m4 input
```

without requiring an impossible or expensive same-lane `u32` vector group.

For 128-byte chunks:

```text
structural word 0 -> base + 0
structural word 1 -> base + 64
```

Each word can be emitted independently.

## 6. Dense fallback question

Do not assume the dense path needs to remain vectorized.

Even at 40–70% event density, scalar sequential stores after bit extraction may be
competitive on an in-order CPU if the alternative is a very expensive high-LMUL
`vcompress`.

Benchmark:

```text
scalar ctz only
```

against every proposed dense vector method before preserving complexity.

## 7. Direct operator indexes

A further experiment may skip building an intermediate final structural bitset and directly
emit operator events plus scalar-start events.

Do not do this first. It complicates ordering and filtering by inside-string state.
Establish a fast final-bitset writer first.

## 8. Acceptance conditions

A new writer is promotable when:

- exact output matches current writer;
- streaming behavior is unchanged;
- all RVV tests pass at all VLENs;
- Stage 1 improves on representative corpora;
- structural-dense inputs do not suffer unacceptable regression;
- code is simpler or performance benefit justifies added complexity.

Record the chosen crossover threshold and the hardware used to derive it.

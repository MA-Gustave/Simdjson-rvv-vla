# Target Stage 1 Fast Pipeline

## 1. Design goal

Stage 1 should separate two fundamentally different tasks:

1. **wide regular byte processing**, which RVV handles well;
2. **irregular event/control processing**, which should not force high-LMUL permutation
   operations across the full byte vector.

The target architecture is therefore:

```text
load wide byte group
        |
        +--> UTF-8 status
        |
        +--> structural/whitespace masks
        |
        +--> quote mask
        |
        +--> backslash mask
        |
        v
compact control representation
(mask bits / 64-bit words)
        |
        +--> escape-run logic
        +--> quote prefix parity
        +--> scalar-start propagation
        +--> inside-string filtering
        |
        v
final structural bitset
        |
        +--> sparse scalar/event writer
        \--> optional dense vector writer
```

The byte width and structural-index width should no longer be mechanically coupled.

## 2. Why this matters

The current `e8m2 -> e32m8` design makes structural emission convenient, but it makes the
most irregular operation use the widest register group in the loop.

The alternative is to spend a small, predictable cost to materialize control masks into a
compact form and then use scalar bit operations for control flow.

This can potentially unlock a wider byte loop without also widening `vcompress` and
`vrgather`.

## 3. Candidate A: current architecture

Keep as permanent baseline:

```text
A0
byte shape: e8m2
quote: vector viota parity
escape: vector run algorithm
scalar-start: element slide
writer: sparse count==1 else u32m8 compress
```

Never delete this path until all replacements have native evidence.

## 4. Candidate B: e8m2 + bitset control plane

First redesign should retain the same byte width:

```text
B1
byte shape: e8m2
classification: unchanged
UTF-8: unchanged
quote/backslash masks -> compact bitsets
string/escape/scalar-start -> bit operations
writer -> adaptive bitset writer
```

This isolates whether the control-plane redesign itself wins.

It is the safest and most informative first performance rewrite.

## 5. Candidate C: wider byte pipeline

Only after Candidate B is working:

```text
C1
byte shape: e8m4
classification: wide or 2x/4x m1 sub-kernels
UTF-8: chosen independently
control masks -> 64-bit words
writer -> bitset/event path
```

On X60/VLEN=256, e8m4 means 128 bytes per outer iteration.

The important property is that C1 must **not** simply replace:

```text
u32m8 writer
```

with an even more permutation-heavy design. Wide input should amortize cheap byte/mask
operations, while irregular control stays compact.

## 6. 64-bit control-word decomposition

For VLA, do not assume one vector mask fits in one machine word.

Represent a logical predicate as a sequence of 64-bit words:

```text
mask word 0 -> lanes   0..63
mask word 1 -> lanes  64..127
...
```

Each word carries a small amount of state into the next word:

- `in_string`;
- escape carry;
- previous scalar bit;
- possibly UTF-8 carry if that component is reorganized.

This maintains VLA semantics while permitting efficient scalar prefix/bit operations.

## 7. Pipeline ordering

A likely efficient ordering is:

```text
1. load bytes
2. derive raw operator / whitespace / quote / backslash / control masks
3. perform ASCII status for UTF-8
4. export only masks needed by scalar control plane
5. resolve escaped quotes
6. prefix-XOR quote bits to get inside-string bits
7. derive scalar starts using a one-bit shift in packed form
8. filter potential structurals by inside-string
9. emit indexes
10. run non-ASCII UTF-8 slow work when required
```

This ordering is a hypothesis. In particular, UTF-8 scheduling must be measured because
fusing it may increase register pressure.

## 8. Register-pressure rule

High LMUL reduces the number of independently addressable register groups.

When testing e8m4:

- inspect spills;
- inspect compiler-created moves;
- inspect vtype changes;
- inspect whether LUT classification requires too many temporary groups;
- measure code with and without fused UTF-8.

If e8m4 creates spills or forces serialized permutation operations, e8m2 may remain the
correct default.

## 9. Definition of success

The Stage 1 redesign succeeds if it produces:

- lower cycles/byte on real hardware;
- equal correctness;
- equal streaming semantics;
- no pathological regression on quote-heavy or escape-heavy JSON;
- maintainable VLA code;
- a clear explanation of why it wins.

A wider `vl` alone is not success.

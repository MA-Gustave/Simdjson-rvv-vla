# Minify Optimization Plan

## 1. Current implementation

`src/rvv/minify.h` already has several strong choices:

- `e8m4` byte processing;
- direct whitespace comparisons;
- quote/backslash-aware semantics;
- bypasses `vcompress` when nothing must be removed.

This makes minify a useful testbed for wide byte LMUL independent of Stage 1's index
writer.

## 2. Main current risk

The expensive part is not necessarily whitespace compaction.

The current string scanner for the m4 path can enter an `u16m8` escape-run algorithm.
On an X60-class machine, high-LMUL permutation operations are exactly what published
microbenchmarks warn about.

Therefore optimize the **string-control plane first**.

## 3. Candidate M0 — current minify

Permanent baseline.

## 4. Candidate M1 — packed quote/backslash control

Reuse the bitset strategy from Stage 1:

```text
quote mask
backslash mask
    ->
escaped quote mask
inside-string mask
    ->
remove = whitespace & ~inside
```

The byte vector remains `e8m4`.

This gives a clean experiment:

```text
same byte width
same whitespace classification
same output compress
different string-state algorithm
```

## 5. Candidate M2 — adaptive output compression

Current behavior:

```text
if remove == 0:
    store original bytes
else:
    keep = ~remove
    vcompress
    store compressed bytes
```

This is already sensible.

Still benchmark alternatives for whitespace-heavy input:

- current `vcompress`;
- copy runs between removed whitespace spans;
- scalar/store-assisted sparse deletion.

The ordinary JSON path should not become worse merely to improve pretty-printed
whitespace-heavy input.

## 6. Candidate M3 — lower-LMUL output slices

If m4 `vcompress` itself is too expensive on a target CPU, keep m4 classification but
compress/store in smaller logical slices.

Example:

```text
classify 128 bytes once
compress 32-byte or 64-byte subranges
```

This trades more instructions for cheaper permutations.

It is particularly plausible on X60, where permutation cost grows sharply with LMUL.

## 7. Corpus classes

Minify must be benchmarked on at least:

- already-minified JSON;
- normal pretty-printed JSON;
- extremely whitespace-heavy JSON;
- long strings containing spaces;
- quote-heavy object data;
- escape-heavy strings;
- multilingual UTF-8.

Report both input GB/s and output ratio.

## 8. Special opportunity: already-minified data

When no removable whitespace exists, minify can be close to a copy plus string-state scan.

Measure whether a very cheap "candidate whitespace exists" check can bypass additional
work without adding too much cost to the general path.

## 9. Correctness constraints

Never remove whitespace inside strings.

Test:

- escaped quotes;
- even/odd backslash runs;
- whitespace immediately around quotes;
- whitespace-like bytes in UTF-8;
- boundary-crossing strings;
- final unclosed string error behavior.

Do not compare partial destination bytes after an error unless the API guarantees them.

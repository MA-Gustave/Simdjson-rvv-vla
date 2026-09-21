# Quote, Backslash and String-State Optimization

## 1. Current state

`src/rvv/string_scanner.h` currently has two expensive conditional paths:

- backslash-run analysis;
- quote-prefix parity.

Both are correct VLA implementations. Both can involve operations whose cost scales with
LMUL.

JSON contains many quotes, so quote handling must be treated as a mainstream hot path.

## 2. Desired control-plane model

For each 64-bit logical word derive:

```text
quote_bits
backslash_bits
```

Then calculate:

```text
escaped_bits
unescaped_quote_bits
inside_string_bits
outgoing_escape_state
outgoing_in_string_state
```

using compact integer operations where profitable.

## 3. Backslash fast path

Mandatory fast path:

```text
if (backslash_bits == 0) {
    escaped_bits = incoming_escape ? 1 : 0
    outgoing_escape = false
}
```

This should be the overwhelmingly cheap path.

If `incoming_escape` is set, only bit 0 is escaped in a no-backslash word.

## 4. Sparse run processing

For a word containing backslashes, process contiguous runs by event position rather than
launching a dense vector permutation graph.

Conceptual algorithm:

```text
while (backslash_bits) {
    start = ctz(backslash_bits)
    determine run length
    mark every second backslash in the run as active escape
    remove run from backslash_bits
}
escaped_bits = active_backslash_bits << 1
```

Carry rules must correctly handle a run beginning at bit 0 that continues from the
previous word.

This path should be compared against the current vector path over a sweep of:

- isolated backslashes;
- short runs;
- long runs;
- boundary-crossing runs;
- dense synthetic escapes.

## 5. Dense escape fallback

The current vector algorithm may remain useful for high escape density.

Possible policy:

```text
few backslashes -> scalar word algorithm
many backslashes -> current vector algorithm
```

Do not add density branching unless measurement shows a real crossover.

## 6. Quote parity with scalar prefix XOR

After escaped quotes are removed:

```text
unescaped_quote = quote_bits & ~escaped_bits
```

inside-string state is a prefix parity problem.

Candidate implementation:

```text
prefix = unescaped_quote
prefix ^= prefix << 1
prefix ^= prefix << 2
prefix ^= prefix << 4
prefix ^= prefix << 8
prefix ^= prefix << 16
prefix ^= prefix << 32
```

Then adjust for incoming `in_string`.

The exact output mask must be aligned with the current `string_tail` semantics:

```text
inside
inside XOR quote
```

Create a truth-table test before integrating into Stage 1.

## 7. Why prefix XOR is attractive

The current quote path uses `viota` widened into element vectors. On X60, `viota` cost
scales with LMUL. Scalar prefix XOR cost is fixed per 64-bit word.

This is especially attractive once the outer byte loop widens beyond 64 lanes: extra
vector lanes add only extra 64-bit control words, not wider prefix-scan vectors.

## 8. Scalar-start detection

Current Stage 1 turns scalar predicates into byte flags and performs an element slide.

In packed form:

```text
follows_scalar = (nonquote_scalar << 1) | incoming_prev_scalar
scalar_start = scalar & ~follows_scalar
```

Carry out:

```text
outgoing_prev_scalar = top valid bit of nonquote_scalar
```

This exactly expresses the logical dependency and removes a wide element slide.

## 9. Error masks

Unescaped control characters inside strings become:

```text
bad_control = control_bits & inside_string_bits
```

The chunk error condition is then a scalar `bad_control != 0`.

This may remove another vector-to-scalar reduction.

## 10. Correctness matrix

Test every combination around boundaries:

```text
...\"...
...\\"...
...\\\"...
...\\\\\"...
```

and corresponding even/odd backslash runs.

Mandatory locations:

- lane 0;
- lane 63;
- word boundary;
- vector boundary;
- final partial vector;
- streaming chunk boundary.

Compare structural-index output, not merely parse success.

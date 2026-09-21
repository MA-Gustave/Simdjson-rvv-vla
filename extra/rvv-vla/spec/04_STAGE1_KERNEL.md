# Stage 1 Kernel Design

This document specifies Stage 1 semantics and the candidate implementation families. The semantic result is fixed; LMUL, prefix method, sparse/dense thresholds and exact intrinsic sequence are tunable.

## 1. Cross-chunk state

Conceptually preserve:

```text
struct rvv_stage1_state {
  bool prev_in_string;
  bool next_is_escaped;
  bool prev_nonquote_scalar;
  bool unescaped_control_error;
  utf8_state utf8;
};
```

Exact layout/names may differ. State must be sufficient to make chunking invisible to parser semantics.

## 2. Chunking contract

For every iteration:

```text
vl = vsetvl_<selected e8 LMUL>(remaining_or_subchunk)
bytes = vle8(..., vl)
```

The selected LMUL is a performance hypothesis. Mandatory candidates are defined in `21_PERFORMANCE_HYPOTHESES.md`.

No normal load may require reading beyond the valid input contract merely to fill a fixed hardware-sized vector. If an internal algorithm uses a bounded semantic subchunk, it must remain correct for all supported VLEN values.

If a lane-index representation would overflow its element width, split the operation before overflow rather than relying on unrealistic hardware assumptions.

## 3. Classification semantics

Derive predicates equivalent to:

```text
quote      = bytes == '"'
backslash  = bytes == '\\'
whitespace = bytes in {0x20, 0x09, 0x0A, 0x0D}
operators  = bytes in {'{','}','[',']',':',','}
control    = bytes <= 0x1F
```

Direct compares, table/gather classification, or another equivalent formulation may be benchmarked. Classification must work for arbitrary active `vl`.

## 4. Escape processing

### 4.1 Mandatory fast path

If no active lane is a backslash:

- if `next_is_escaped` is false, `escaped` is empty;
- if true, lane 0 is escaped and the continuation state is consumed;
- do not run dense run-analysis machinery.

### 4.2 Candidate E1 — rare-event/run path

For low backslash counts, a bounded scalar/event-driven path is allowed. It may use `vfirst`, compacted positions, or direct inspection of identified runs. The prohibition is against scalar processing of every byte, not against scalar handling of rare events.

This path must handle a run continuing from the previous chunk and produce the exact escaped-byte mask/state.

### 4.3 Candidate E2 — dense vector run-parity path

For escape-heavy chunks, a vector path may use lane IDs, run starts, `viota`, compact/gather, lane arithmetic and shifts to derive run parity. This is the original v1.0 design hypothesis, retained as a candidate rather than a lock.

### 4.4 Adaptive selection

An implementation may select E1/E2 using `vcpop(backslash)` or another cheap density signal. Thresholds are benchmark parameters, not magic constants.

### 4.5 Required escape tests

Exhaustively cover backslash runs of at least 1..16 around every relevant boundary, including lane 0, lane `vl-1`, runs crossing vector boundaries, quotes, normal bytes, string/outside-string cases, and very small `vl` values including 1.

## 5. Unescaped quotes

```text
unescaped_quote = quote & ~escaped
```

Every later string-state computation uses unescaped quotes.

## 6. Inside-string state

Required semantic rule:

- opening quote lane is considered inside for the generic string-tail convention;
- closing quote lane transitions to outside;
- parity carries across chunks.

Candidate Q1 uses `viota(unescaped_quote)` plus the current quote bit to obtain inclusive parity. Candidate Q2 may use another scalable prefix mechanism. Exact choice is benchmark-selected.

The chunk-end scalar state is updated from the parity of the number of unescaped quotes.

The result must reproduce generic `string_tail` semantics exactly.

## 7. Pseudo-structural detection

Equivalent semantics:

```text
scalar = ~(operators | whitespace)
nonquote_scalar = scalar & ~unescaped_quote
follows_nonquote_scalar = shift(nonquote_scalar, incoming=prev_nonquote_scalar)
potential_scalar_start = scalar & ~follows_nonquote_scalar
potential_structural = operators | potential_scalar_start
structural = potential_structural & ~string_tail
```

The lane shift must be vectorized or event-equivalent; do not scalarize every lane. Update `prev_nonquote_scalar` from the final active lane.

## 8. Unescaped control-character errors

Equivalent to the frozen generic Stage 1 behavior: literal control bytes `<= 0x1F` occurring inside strings are errors. Accumulate a boolean/error mask without changing normal structural output.

## 9. Structural-index emission

The parser consumes `uint32_t` absolute indexes. Three development candidates are allowed:

### W1 — native vector compaction

Conceptually:

```text
lane = vid
packed = vcompress(lane, structural)
count = vcpop(structural)
convert packed offsets to uint32_t
add chunk base
store count indexes contiguously
```

For byte LMUL=m1, a natural same-lane chain is `u16m2 -> u32m4`. For byte LMUL=m2, `u16m4 -> u32m8` is natural. These relationships are candidates, not product API.

### W2 — sparse event writer

For very small structural counts, repeatedly extracting only the actual events may avoid setting up wide index vectors. A bounded per-structural/event loop is allowed as an adaptive fast path. It must not degenerate into a byte loop.

### W3 — reference/ablation writer

Keep a simple correctness/reference writer in development builds/bench harnesses. It exists to prove whether W1/W2 creates the gain; it is not automatically a shipping mode.

### Adaptive writer

A density-based W2/W1 switch is allowed and expected to be evaluated. Thresholds are hardware evidence, not architecture law.

## 10. UTF-8 scheduling

The loaded byte stream must be validated exactly. Scheduling options are described in `05_UTF8_MINIFY_STAGE2.md` and benchmarked separately. Stage 1 semantics cannot depend on which schedule wins.

## 11. Streaming and finalization

Preserve frozen v4.6.11 behavior for capacity, empty input, partial UTF-8 trimming, regular/streaming-partial/streaming-final modes, unclosed strings, structural sentinels, `n_structural_indexes`, truncated-document logic and parser bookkeeping.

Prefer reusing existing helpers instead of reimplementing document-stream rules.

## 12. Debug/reference instrumentation

Development builds may expose:

- per-chunk predicate dumps;
- structural-index differential checks;
- algorithm-candidate IDs;
- reference writer selection;
- UTF-8 schedule selection.

None of this may change release API or produce output from core library code.

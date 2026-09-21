# UTF-8, Minify, Stage 2 and Utility Kernels

## 1. UTF-8 correctness contract

The `rvv` backend must provide complete UTF-8 validation for both `implementation::validate_utf8` and parser Stage 1. ASCII-only acceleration followed by a generic check of all remaining data is not considered a complete optimized implementation.

The current simdutf RVV validator is a useful algorithm/reference source: ASCII fast path, table-driven neighboring-byte classification, gathers/slides, mask validation and a small scalar tail/error path. Any close adaptation requires explicit license/provenance review.

## 2. Standalone validator candidates

The standalone API is free to choose a different LMUL from Stage 1 because register pressure is different. Candidate LMULs and table/gather formulations are performance hypotheses. Correctness and exact error behavior are mandatory.

## 3. Parser UTF-8 scheduling candidates

Evaluate at least the following when practical:

- **U1 same-loop/fused:** validate using the loaded Stage 1 data while it is live;
- **U2 adjacent/software-pipelined:** keep UTF-8 in the Stage 1 traversal but shorten overlapping live ranges, potentially with adjacent vector loads;
- **U3 second-pass reference:** separate complete validator after/before structural scan, used as a correctness/performance reference and allowed to ship only if end-to-end evidence proves it superior enough to justify the extra pass.

Do not assume one memory pass is automatically faster if it causes spills or blocks useful LMUL choices.

All schedules must handle multibyte sequences crossing chunk boundaries and maintain identical parser-level error semantics.

## 4. Minify correctness

JSON whitespace is removed only outside quoted strings. Whitespace inside strings is data. The old prototype that removed all whitespace is rejected.

Minify should reuse the same tested quote/escape semantics as Stage 1, either through a shared internal helper or an independently compiled equivalent with shared differential tests.

Conceptually:

```text
remove = json_whitespace & outside_string
keep = ~remove
```

## 5. Minify store candidates

- **M1 `vcompress`:** compact kept bytes and vector-store the prefix; primary RVV hypothesis.
- **M2 copy-runs/event path:** optional candidate if a target implements compress poorly or JSON has long keep-runs.

Selection is benchmark-driven. Exact byte output/error behavior is invariant.

## 6. Generic Stage 2 reuse

Use generic tape construction, Stage 2, Stage 2-next and number parsing until profiling demonstrates a meaningful separate opportunity. Stage 1 performance work must not balloon into an unrelated parser rewrite.

## 7. String parsing adapter

Where generic Stage 2 expects a fixed semantic `BYTES_PROCESSED` helper, a 64-byte semantic window is acceptable. Internally implement loads/stores/searches with VLA RVV and `vfirst`/equivalent. This helper boundary does not constrain Stage 1 chunking.

Required behavior includes first quote/backslash/control detection, copy semantics, wobbly-string behavior and padding assumptions matching v4.6.11.

## 8. Number parsing

Start from the current proven generic/SWAR number-parsing definitions. RVV-specific number kernels are outside the initial performance thesis unless profiling after Stage 1 completion shows a worthwhile hotspot.

## 9. On-Demand and builder integration

Expose the implementation-specific umbrella headers required by v4.6.11 while reusing generic On-Demand/builder behavior unless a separate measured optimization is introduced.

## 10. Utility parity tests

Compare `rvv` to fallback and, where useful, `rvv_vls` for valid/invalid UTF-8, minify exact output, string/wobbly parsing, numeric edges, DOM and On-Demand results.

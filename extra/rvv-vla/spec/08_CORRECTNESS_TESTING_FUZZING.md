# Correctness, Testing and Fuzzing

## 1. Correctness principle

Performance work is accepted only after differential correctness against trusted implementations.

Primary references:

- fallback backend for architecture-independent semantics;
- `rvv_vls` for current RISC-V optimized behavior where the fixed-VLEN build is available.

## 2. VLEN correctness matrix

Minimum QEMU matrix:

```text
128
256
512
1024
```

The same source backend must pass all VLEN values. No source edit or fixed-vector-bits rebuild is allowed to change the algorithm's semantics.

## 3. Compiler matrix

At minimum:

- current supported GCC RISC-V toolchain;
- current supported Clang RISC-V toolchain.

Compiler-specific intrinsic workarounds must be isolated and tested under both.

## 4. Stage 1 differential tests

Compare structural indexes exactly against fallback/reference for generated and curated JSON.

Required categories:

- empty/one-token/small documents;
- nested arrays/objects;
- dense numeric arrays;
- boolean/null-heavy arrays;
- whitespace-heavy documents;
- long strings;
- quote-heavy strings;
- backslash-heavy strings;
- every backslash-run length 1..8;
- backslash runs crossing every plausible vector boundary;
- escaped quotes at lane 0 and lane `vl-1`;
- control characters inside strings;
- scalars beginning at lane 0;
- scalar runs crossing a vector boundary;
- operators at lane boundaries;
- input lengths around VLMAX-1/VLMAX/VLMAX+1;
- input lengths around 63/64/65 even though Stage 1 is no longer 64-byte based;
- large documents.

## 5. Streaming tests

Exercise:

- regular mode;
- streaming partial;
- streaming final;
- truncated multibyte UTF-8 at buffer ends;
- unclosed strings;
- document boundaries around vector boundaries;
- trailing garbage behavior matching existing simdjson.

## 6. Escape-state exhaustive microtests

For a small window, enumerate all combinations of backslash/quote positions up to a tractable width and compare the VLA string-state helper against a scalar reference state machine.

This test is more valuable than testing only whole JSON documents because it directly validates the hardest vector-state algorithm.

## 7. UTF-8 tests

Use existing simdjson Unicode tests plus RVV-specific boundary generation.

Must include:

- ASCII;
- valid 2/3/4-byte sequences;
- overlong encodings;
- surrogate encodings;
- values above U+10FFFF;
- stray continuation bytes;
- truncated sequences;
- every sequence type crossing vector boundaries;
- chunks ending with 1/2/3 bytes of a multibyte sequence;
- random byte strings differential-tested against fallback.

## 8. Minify tests

Require exact byte-for-byte output and error code equivalence.

Critical cases:

```json
{"a":"space stays here","b": 1}
{"a":"\\\" quoted ", "x" : [ 1 , 2 ] }
```

Also test backslash runs and whitespace at every vector boundary.

## 9. DOM and On-Demand tests

Run the full existing test suites with `SIMDJSON_FORCE_IMPLEMENTATION=rvv` where supported.

RVV-specific tests should verify that forcing actually selected `rvv`, not silently fallback.

## 10. Number/string parsing tests

Reuse existing generic tests plus RVV smoke tests. Because Stage 2 is mostly reused, regressions usually indicate integration/adapter mistakes.

## 11. Differential fuzzing

### Parse fuzz

For each fuzz input:

- run fallback;
- run `rvv`;
- compare success/error and normalized parse behavior within public semantic guarantees.

### Minify fuzz

Compare error code, output length and bytes.

### UTF-8 fuzz

Compare boolean validity (and error position if testing an API that exposes it).

## 12. Sanitizers

Where host/toolchain support permits:

- ASan;
- UBSan;
- debug iterator/development checks.

QEMU cross builds may not support every sanitizer reliably; document limitations rather than skipping silently.

## 13. No performance shortcuts in tests

Test code may expose internal debug functions behind development-only macros, but production algorithms must not be weakened to make testing easier.

## 14. Required pre-merge correctness gate

A code change touching Stage 1/string state cannot be considered complete until:

- targeted microtests pass at all four VLENs;
- RVV unit tests pass under GCC and Clang matrix where available;
- full CTest passes for at least the primary cross toolchain;
- relevant fuzz smoke count passes;
- non-RISC-V native CI remains green.

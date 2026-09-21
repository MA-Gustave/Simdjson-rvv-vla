# Algorithm Invariants

These are the non-negotiable semantic properties of the backend. They intentionally avoid prescribing a particular LMUL or instruction sequence.

## Stage 1

**I-S1-001** Every byte in the effective input window is classified exactly once for the parser semantics; chunk boundaries are invisible.
**I-S1-002** Active `vl` may vary every iteration; correctness does not depend on fixed hardware VLEN.
**I-S1-003** Loads/stores stay within the valid simdjson memory/padding contract.
**I-S1-004** JSON whitespace is exactly `0x20, 0x09, 0x0A, 0x0D`.
**I-S1-005** JSON operators are exactly `{ } [ ] : ,`.
**I-S1-006** Scalar-start/pseudo-structural output is byte-identical to frozen v4.6.11 semantics.
**I-S1-007** Structural indexes are absolute `uint32_t` positions in strictly increasing input order.
**I-S1-008** Structural-buffer capacity/sentinel bookkeeping matches frozen parser behavior.
**I-S1-009** Empty/capacity/streaming error results match frozen behavior.

## Strings and escapes

**I-STR-001** Only unescaped `"` toggles in-string state.
**I-STR-002** Backslash-run parity is exact across every vector/chunk boundary.
**I-STR-003** An escape continuation from the previous chunk is represented correctly at lane 0.
**I-STR-004** Opening/closing quote treatment reproduces generic `string_tail` semantics.
**I-STR-005** No production path performs scalar work for every byte merely to determine escape/string state.
**I-STR-006** Literal control bytes `<=0x1f` inside strings produce the same unescaped-control error behavior as frozen Stage 1.

## UTF-8

**I-U8-001** All accepted byte sequences are valid UTF-8 and all invalid sequences rejected according to simdjson behavior.
**I-U8-002** Sequences crossing VLA chunk boundaries are validated correctly.
**I-U8-003** Streaming partial trimming/finalization remains compatible with frozen parser semantics.
**I-U8-004** Standalone `validate_utf8` and parser validation agree on validity.

## Minify

**I-MIN-001** Only JSON whitespace outside strings is removed.
**I-MIN-002** All bytes inside strings, including spaces and escaped content, are preserved byte-for-byte.
**I-MIN-003** Output order is stable and contiguous.
**I-MIN-004** Error/unclosed-string behavior matches simdjson's contract.

## Stage 2/public behavior

**I-S2-001** DOM and On-Demand observable results/errors match fallback for the same input.
**I-S2-002** String unescape/wobbly parsing behavior matches the frozen implementation contracts.
**I-S2-003** Number parsing does not change public numeric semantics.
**I-API-001** No new parser API is introduced for RVV.
**I-API-002** Non-RISC-V builds do not require RISC-V headers/options.

## Dispatch/build

**I-BLD-001** Baseline code never executes RVV instructions on runtime hardware lacking V in a build mode that claims dispatch safety.
**I-BLD-002** Sizeless vector types do not cross the mixed-ISA ABI boundary.
**I-BLD-003** `rvv_vls` remains independently buildable/reference-capable.
**I-BLD-004** Single-header support claims exactly match tested compiler/build modes.

## Measurement/process

**I-PROC-001** QEMU timing is never used as speed evidence.
**I-PROC-002** A performance choice is not promoted without repeatable real-hardware evidence.
**I-PROC-003** AI-generated code/text is human reviewed and explainable before external review.

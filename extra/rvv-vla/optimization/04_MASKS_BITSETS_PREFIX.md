# Masks, Bitsets and Prefix Operations

## 1. Problem statement

simdjson Stage 1 is naturally expressed as bitset algebra:

- quote positions;
- escaped positions;
- inside-string state;
- operators;
- whitespace;
- scalar starts;
- structural positions.

Fixed-width SIMD backends often reduce these predicates to integer masks and then use
cheap scalar bit operations.

RVV is predicate-oriented, but VLA makes "just reinterpret the mask as a uint64" unsafe as
a universal design.

We need a scalable bridge between RVV predicates and compact bit operations.

## 2. Proposed representation

Use 64-bit logical mask words independent of VLEN:

```text
struct mask_word_state {
    uint64_t quote;
    uint64_t backslash;
    uint64_t op;
    uint64_t whitespace;
    uint64_t control;
    unsigned valid_bits;
};
```

This is conceptual, not necessarily the final C++ API.

For a vector chunk larger than 64 lanes, process multiple words. For a tail, mask off bits
outside `valid_bits`.

## 3. Predicate-to-bitset extraction

Candidate mechanisms to benchmark:

### M1 — `vsm.v` to a small stack/local buffer

RVV has a dedicated mask store. Published X60 measurements indicate `vsm.v` is relatively
cheap compared with high-LMUL gather/compress operations.

Possible flow:

```text
RVV predicate
    -> vsm.v temporary bytes
    -> load uint64_t word(s)
```

Advantages:

- naturally VLA;
- no dependence on a fixed VLEN;
- mask bits are already packed.

Risks:

- store/load round trip;
- stack aliasing / compiler scheduling;
- endian/bit-order validation;
- tail handling.

### M2 — mask reinterpret/extract for <=64 lanes

For chunks known to contain at most 64 lanes, it may be possible to use a register
reinterpretation/extraction path without memory.

This must remain an optimization, not an assumption baked into VLA correctness.

### M3 — process the outer loop as 64-lane logical blocks

Use RVV to compute exactly 64 logical lanes where possible, even if hardware VLEN is
larger, and use scalar masks directly.

This simplifies the control plane but may leave vector capacity unused. It should be
benchmarked only as a reference / portability candidate.

## 4. Prefix XOR for quote parity

Given a bit word `q` containing **unescaped quote positions**, the inside-string mask is a
prefix XOR of quote events combined with the incoming `in_string` state.

A standard scalar 64-bit prefix XOR can be implemented by repeated shifts:

```text
x ^= x << 1
x ^= x << 2
x ^= x << 4
x ^= x << 8
x ^= x << 16
x ^= x << 32
```

The exact definition of "inside at quote lane" versus "inside after quote lane" must match
simdjson's current semantics. Do not substitute this blindly.

The final outgoing `in_string` state is simply the parity of quote count in the word.

This replaces widened element flags + `viota` + vector parity arithmetic with integer
operations whose cost does not grow with LMUL.

## 5. One-bit predicate shifts

Several Stage 1 operations need "the state of the previous byte".

In bitset form:

```text
shifted = (mask << 1) | incoming_bit
```

with carry from one 64-bit mask word to the next.

This is a direct representation of:

- follows-nonquote-scalar;
- escaped-next-byte;
- other one-byte dependency masks.

It avoids using `vslide1up` on byte/word elements merely to move one predicate bit.

## 6. Escape runs

Backslash semantics require alternating parity within each contiguous run.

The bitset algorithm should operate only on words containing backslashes. Candidate
approaches:

- scalar event loop using `ctz`;
- run extraction by `ctz` plus count-trailing/leading operations;
- carry a single "run continues with odd parity" state across words.

The common case should be:

```text
backslash_bits == 0
```

which becomes an extremely cheap branch.

Do not optimize dense pathological escape input before the sparse real-world path.

## 7. Mask-word API invariants

Any reusable mask-word helper should document:

- bit `i` maps to byte lane `i`;
- tail bits are zero;
- exact meaning of carry-in and carry-out;
- quote position inclusion/exclusion semantics;
- no undefined shifts by 64;
- no unaligned aliasing assumptions unless proven safe.

## 8. Validation strategy

For every mask primitive, add deterministic tests for:

- bit 0;
- bit 63;
- transitions across word boundaries;
- transitions across vector iterations;
- VLEN 128/256/512/1024;
- final short tail;
- quote directly after backslash;
- long odd/even backslash runs crossing boundaries.

A bitset path is only useful if it makes state boundaries easier to reason about, not
harder.

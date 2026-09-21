# Current Hotspots in the Correctness-First Backend

This file maps the current implementation to the operations most likely to matter during
native profiling.

## 1. Stage 1 main loop

Current file:

```text
src/rvv/stage1.h
```

The loop currently processes bytes with `e8m2`:

```text
vl = vsetvl_e8m2(...)
load bytes
UTF-8 consume
escape detection
quote detection
inside-string calculation
classification
scalar-start calculation
control-character detection
structural index emission
```

On VLEN=256, `e8m2` gives 64 byte lanes per iteration.

This is a sensible correctness-first shape because `e32m8` has the same lane count, making
direct 32-bit structural-index compaction possible. It also couples the byte-processing
width to the structural-output width, which may unnecessarily constrain performance.

## 2. Structural-index writer

Current function:

```text
json_structural_indexer::write_indexes()
```

Current policy:

- `count == 0`: return;
- `count == 1`: `vfirst`, scalar store;
- otherwise:
  - `vid` as `u32m8`;
  - `vcompress` as `u32m8`;
  - vector add of base;
  - vector store.

This is probably the highest-priority performance suspect on X60.

Why:

- the writer creates a very wide 32-bit register group;
- wide `vcompress` is known to scale poorly on X60;
- ordinary JSON often has a modest number of structurals per 64-byte block, so a full
  vector compaction may be overkill.

The key question is the crossover point between:

- scalar bit extraction;
- small-event handling;
- vector compaction.

## 3. Escape processing

Current files:

```text
src/rvv/string_scanner.h
```

The no-backslash fast path is good: a mask check avoids the heavy algorithm.

When backslashes are present, the current path uses a sequence including:

- `vid`;
- `vcompress`;
- `viota`;
- `vrgather`;
- subtractions and parity;
- slide to produce escaped lanes.

For Stage 1 this occurs in `u16m4`. In minify it can occur in `u16m8`.

This is exactly the class of high-LMUL permutation sequence that deserves replacement or
an adaptive sparse path.

Most real JSON does not have dense backslash runs. The architecture should exploit event
sparsity rather than paying a dense vector-run algorithm as soon as one backslash occurs.

## 4. Quote-prefix / inside-string processing

Current function:

```text
inside_string()
```

The no-quote fast path is cheap.

When quotes exist, the current path:

- converts quote mask to element flags;
- runs `viota`;
- adds current quote;
- computes parity;
- optionally XORs incoming string state;
- converts parity back to a mask;
- performs `vcpop` to update carry state.

This is correct and VLA, but quotes are common in JSON. A path that is "rare" for arbitrary
text is not rare for JSON.

The central experiment should compare this against packed-bit prefix XOR over the quote
mask.

## 5. Scalar-start propagation

Stage 1 currently turns a predicate into byte flags, then performs:

```text
vslide1up
```

to represent "the previous lane was a non-quote scalar".

This is logically a **one-bit predicate shift**, not a byte-vector transformation.

Olaf Bernstein's RVV gap analysis specifically identifies mask-slide operations as a weak
spot in current RVV. We should test a bitset representation or another mask-native
construction instead of paying for an element slide.

## 6. Character classification

Current functions:

```text
classify_whitespace()
classify_operator()
detail::lookup16_u8m2()
```

The algorithm uses compact 16-byte lookup tables, which is good algorithmically.

Because RVV does not provide a cross-register LMUL>1 16-entry gather, our helper splits
the `m2` index vector into two `m1` pieces and performs two `m1` gathers.

This is **not automatically a hotspot to remove**. The published X60 results show that
LMUL=1 gather is much cheaper than high-LMUL gather, so the split approach is aligned with
known microarchitectural behavior.

Experiments should still compare:

- current split-m1 LUT classification;
- direct compares;
- an explicitly unrolled m1 pipeline that avoids helper/vtype overhead.

Do not replace the LUT purely on intuition.

## 7. UTF-8 validation inside Stage 1

Current file:

```text
src/rvv/utf8_validation.h
```

Positive properties:

- ASCII fast path;
- uses the already-loaded Stage 1 byte vector;
- only three scalar carry bytes across VLA chunks;
- lookup4 structure follows established fast UTF-8 techniques.

Potential costs:

- ASCII detection requires a vector-to-scalar mask query;
- non-ASCII path performs several slides and three lookup tables;
- each m2 lookup is split into m1 gathers;
- helper structure may cause additional `vsetvli` changes depending on compiler codegen.

The first optimization task here is assembly inspection, not rewriting the algorithm.

## 8. Minify

Current file:

```text
src/rvv/minify.h
```

Good existing choices:

- `e8m4` gives a large byte chunk;
- direct whitespace compares avoid lookup gathers;
- `vcompress` is skipped when there is nothing to remove.

Main suspect:

- escape handling reuses the heavy m4/m8 string-scanner machinery.

Minify should therefore be optimized primarily through cheaper quote/backslash state, not
by immediately replacing its store/compress design.

## 9. Cross-cutting risk: vtype churn

Helpers switch between multiple element widths and LMULs:

- e8m2;
- e8m1 lookup segments;
- u16m4;
- u32m8;
- e8m4 / u16m8 in minify.

The compiler inserts `vsetvli` as needed. The number and placement of those transitions
must be inspected in generated assembly.

Add an assembly-count report for:

```text
vsetvli
vcompress
vrgather
viota
vslide
vfirst
vcpop
```

per hot loop before and after each major change.

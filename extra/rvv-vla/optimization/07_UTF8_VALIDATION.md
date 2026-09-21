# UTF-8 Validation Optimization

## 1. Current architecture

`src/rvv/utf8_validation.h` implements a scalable adaptation of the lookup4 UTF-8
algorithm.

Important existing strengths:

- Stage 1 reuses the byte vector already loaded for JSON scanning;
- ASCII chunks use a fast path;
- only three bytes of carry state are kept between VLA chunks;
- non-ASCII validation uses the established three-table lookup4 structure;
- LMUL>1 16-entry lookups are split into LMUL=1 gathers.

The last point is important: published X60 measurements show `vrgather.vv` becoming much
more expensive as LMUL grows. Splitting a larger logical vector into LMUL=1 gathers is a
reasonable performance-portable strategy and should not be discarded casually.

## 2. First task: inspect code generation

Before rewriting the algorithm, collect GCC and Clang assembly and count:

```text
vsetvli
vrgather
vslide
vfirst
vcpop
loads of lookup tables
```

Questions:

- are the 16-byte lookup tables hoisted?
- is `vsetvlmax_e8m1()` recomputed or reconfigured repeatedly?
- does returning/reconstructing `m2` values cause redundant moves?
- how many vtype changes occur per non-ASCII chunk?
- does the compiler spill around the fused Stage 1 path?

Optimization should target observed codegen.

## 3. Candidate U0 — current fused e8m2

Keep as permanent baseline.

## 4. Candidate U1 — explicit two-m1 validation lanes

Instead of expressing lookup4 as an m2 vector split inside each helper, explicitly process
the two m1 halves of an X60-sized 64-byte chunk.

Potential advantages:

- keep lookup tables and gather vtype stable;
- avoid repeated m2 -> m1 -> m2 abstraction transitions;
- make six LMUL=1 gathers explicit;
- simplify scheduling.

Potential disadvantages:

- more source complexity;
- carry between halves must remain correct;
- compiler may already generate equivalent code.

Only assembly + benchmark can decide.

## 5. Candidate U2 — separate Stage 1 UTF-8 pass

A separate validation pass increases memory traffic but can reduce register pressure and
vtype churn in the structural scanner.

Measure:

```text
fused validation
vs
separate validation
```

on:

- small hot-cache files;
- medium files;
- large files exceeding cache;
- ASCII;
- multilingual UTF-8.

Do not assume a fused pass wins. The repo's performance hypothesis already recognizes
this trade-off.

## 6. Candidate U3 — wider standalone validator

Standalone `validate_utf8()` does not need to share Stage 1's LMUL.

Try:

- m1 unrolled;
- m2 current;
- m4 only if its lookup strategy does not invoke expensive high-LMUL gathers.

A wide byte LMUL is useful only if the algorithm keeps permutation work at small LMUL.

## 7. ASCII path

The ASCII fast path is essential for typical JSON.

Measure separately:

- pure ASCII;
- one non-ASCII byte per 64 bytes;
- one per 1 KiB;
- multilingual dense UTF-8.

A fast path whose test serializes every iteration may still lose on some CPUs. Keep a
version that batches the ASCII decision if profiling suggests reduction latency is
dominant.

## 8. Optional scalar xperm4 research path

Olaf Bernstein has demonstrated a scalar SWAR UTF-8 validator using `xperm4` from the
Zbkx/Zbkb family. His work shows that on suitable hardware it can approach vector
validation performance.

This is **not a baseline RVV requirement** and should only be an optional guarded
experiment when the target CPU exposes the required scalar bit-manipulation extension.

Possible uses:

- standalone UTF-8;
- tail validation;
- a secondary microarchitecture-specific path.

Do not make the portable `rvv` backend depend on it.

## 9. Promotion criteria

A UTF-8 change must be tested for:

- all valid UTF-8 lengths;
- overlong encodings;
- surrogates;
- >U+10FFFF;
- isolated continuation bytes;
- incomplete tails;
- errors crossing 64-bit, vector and streaming boundaries.

Performance should be reported separately for ASCII and non-ASCII corpora.

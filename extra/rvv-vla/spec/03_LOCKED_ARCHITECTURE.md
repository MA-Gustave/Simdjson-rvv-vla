# Locked Architecture

This file contains only decisions that should remain stable without performance measurements. Algorithmic choices that may vary by microarchitecture belong in `21_PERFORMANCE_HYPOTHESES.md`.

## A. Backend identity

**D-001 — implementation key:** `rvv`  
**D-002 — namespace:** `simdjson::rvv`  
**D-003 — preserve `rvv_vls`:** it remains a buildable reference/alternative.  
**D-004 — frozen development baseline:** start at v4.6.11 and rebase only at documented checkpoints.

## B. VLA and type safety

**D-010 — no fixed-vector-bit dependency in the VLA Stage 1 core.**

The new Stage 1 must not require `__riscv_v_fixed_vlen` or `riscv_rvv_vector_bits` for correctness.

**D-011 — sizeless RVV types stay local.**

Do not store sizeless RVV vector types as ordinary class/struct members, expose them in public ABI signatures, or persist them across generic boundaries. Scalar state structs are allowed.

**D-012 — RVV 1.0 baseline.**

Optional extensions such as Zvbb may exist only behind guarded, correctness-equivalent fast paths.

## C. Stage 1 architecture

**D-020 — custom VLA Stage 1 hot loop.**

The production backend owns a scalable Stage 1 loop/state machine. It may reuse generic finalization helpers and exact semantics, but it must not make an array-backed/fixed-64-byte `simd8x64` emulation the architectural hot path.

**D-021 — vector shape is benchmark-selected.**

No LMUL is permanently locked before real-hardware evidence. The implementation must be structured so at least the principal m1/m2 candidates can be compared without rewriting parser semantics. A selected default is promoted only through the hypothesis/ADR process.

**D-022 — no universal 64-byte Stage 1 block.**

Correctness is based on active `vl`, not a hardware vector being 64 bytes. Internal subchunks for a specific candidate are allowed only if they preserve the VLA product property.

**D-023 — predicates are first-class.**

Do not require conversion of every Stage 1 predicate to a scalar `uint64_t` as the canonical representation. Scalar event extraction is allowed when a sparse/adaptive algorithm proves faster.

## D. Structural index emission

**D-030 — maintain at least one native vector-compaction writer and one reference/ablation writer.**

The product must support a direct RVV candidate based on lane indexes plus masked compaction. A scalar/event/reference writer must also remain available for correctness and A/B measurement during development. The shipping policy may be vector-only or adaptive after hardware evidence.

A per-byte scalar loop is forbidden. A bounded per-event sparse path is allowed.

## E. String/escape state

**D-040 — cross-chunk state is scalar and minimal.**

Carry only the state needed for correctness, such as in-string parity, escape continuation, previous nonquote-scalar state, and UTF-8 boundary state.

**D-041 — no-backslash fast path is mandatory.**

Chunks with no backslashes must avoid dense escape-run machinery.

**D-042 — escape handling must be scalable and density-aware.**

The final design may use a rare-event path, vector run-parity path, or adaptive combination. It must not scan every byte scalarly. Exact instruction sequences are hypotheses, not locks.

**D-043 — quote parity must be scalable.**

Inside-string state must be derived correctly for arbitrary active `vl` and across chunk boundaries. `viota` is a candidate, not a permanent requirement.

## F. UTF-8

**D-050 — complete UTF-8 correctness.**

The final backend validates all UTF-8, not only ASCII.

**D-051 — UTF-8 integration is benchmark-selected.**

The project must evaluate same-loop/fused, adjacent/software-pipelined, and reference second-pass arrangements as needed. Correctness is locked; the memory-pass/register-pressure trade-off is not. The shipping choice must be justified by end-to-end evidence.

## G. Minify

**D-060 — string-aware minify.**

Whitespace is removed only outside strings; old whitespace-only code is rejected.

**D-061 — minify must include a native RVV compaction candidate.**

`vcompress` is the primary hypothesis, but a different store strategy may win after measurement. Correctness and contiguous output are locked, not one exact instruction.

## H. Stage 2

**D-070 — reuse generic Stage 2 initially.**

Do not vectorize tape construction/number parsing without profiling evidence.

**D-071 — implementation-specific string helpers may use a fixed semantic window.**

A 64-byte semantic helper is acceptable where the generic Stage 2 interface requires it; internally it remains VLA-safe. This does not reintroduce a 64-byte Stage 1 architecture.

## I. Dispatch and build

**D-080 — OS-visible runtime capability detection.**

Prefer Linux `riscv_hwprobe`/standard capability mechanisms. No SIGILL probing.

**D-081 — mixed-ISA isolation must be proven.**

Use a separately compiled RVV translation unit/object as the conservative mode. Function/region target attributes may be used where compiler probes prove them correct. No sizeless-vector ABI crosses the boundary.

**D-082 — single-header support level must be factual.**

Do not promise generic runtime-dispatched single-header support until it compiles and runs on the intended GCC/Clang matrix.

**D-083 — automatic backend priority is evidence-based.**

Do not hard-code `rvv` as universally preferred over `rvv_vls`. In build modes where both are meaningfully selectable, default ordering requires benchmark evidence for the intended deployment class.

## J. Engineering/process

**D-090 — minimal upstream switchboard changes.**  
**D-091 — no unrelated refactors.**  
**D-092 — performance claims require real hardware.**  
**D-093 — QEMU is correctness-only.**  
**D-094 — human review/understanding of AI-generated code is mandatory.**  
**D-095 — performance hypotheses cannot be silently promoted to architecture decisions.**

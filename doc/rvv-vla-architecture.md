# RVV-VLA architecture

## Purpose

`rvv` is the scalable RISC-V Vector backend. It coexists with upstream
`rvv_vls`, which uses compile-time fixed vector lengths.

The scalable backend is built around runtime `vl` selection and keeps RVV
sizeless types confined to implementation-specific code.

## Pipeline

The public parsing model remains simdjson's normal pipeline:

1. Stage 1 identifies structurals and validates input properties.
2. Stage 2 reuses generic simdjson parsing where a vector-specific duplicate
   engine would not provide a compelling advantage.
3. DOM and On-Demand are exposed through the normal implementation object.

The core architectural difference is Stage 1: predicates can remain in vector
or mask form and structural indexes can be emitted directly rather than forcing
all work through a scalar 64-bit structural-mask abstraction.

## Stage 1

Main code:

- `src/rvv/stage1.h`
- `src/rvv/string_scanner.h`

Properties:

- VLA/runtime-VLEN operation
- minimal scalar cross-chunk state
- quote and backslash fast paths
- vector structural classification
- direct structural-index emission
- no required scalar per-byte primary path

The current second-wave implementation also contains bounded packed-mask
control paths. Sparse and packed writers are performance policies rather than
VLA correctness limits; dense vector fallbacks remain available when thresholds
or packed-mask capacity are exceeded.

## UTF-8

`src/rvv/utf8_validation.h` implements scalable UTF-8 validation.

ASCII is a fast path. Non-ASCII input is handled in RVV code while preserving
the state required across chunks for multibyte sequences.

## Minification

`src/rvv/minify.h` is string-aware. JSON whitespace is removed only outside
strings; string contents and escape sequences are preserved exactly.

## Stage 2

Stage 2 remains primarily generic. RVV-specific string and number parsing glue
is supplied where required by simdjson's implementation contract.

## Runtime dispatch

The backend has its own implementation ID and instruction-set bit. Detection is
kept separate from execution so non-RVV systems never enter RVV instructions.
The design does not rely on SIGILL probing.

## Build isolation

Non-RISC-V builds must not require `<riscv_vector.h>`. Intrinsics are isolated to
RVV implementation code so ordinary simdjson builds remain unaffected.

The correctness runner also keeps cross/QEMU and native execution distinct. A
native `riscv64` host uses its host compiler, while non-native correctness runs
use the RISC-V cross toolchain and QEMU emulator.

## Validation and performance isolation

Correctness and performance evidence are deliberately separated:

- QEMU is used to exercise multiple VLEN values and validate parser behavior;
- native RVV 1.0 hardware is required for performance measurements;
- native VLEN is measured by an executable RVV probe rather than assumed from
  the provider description;
- QEMU/TCG-like hosts are rejected by the performance harness by default;
- native results retain compiler, Git, CPU-affinity, virtualization and raw
  benchmark metadata.

`scripts/rvv/native_suite.py` is the high-level native orchestration layer.
It delegates correctness to `scripts/rvv/run_tests.py` and performance to
`scripts/rvv/perf/bench_native.py`; it does not create a second implementation
of either test system.

## Single-header

`singleheader/simdjson.h` and `singleheader/simdjson.cpp` are included in the
snapshot with the backend integrated.

## Invariants vs hypotheses

The authoritative split is documented in:

- `extra/rvv-vla/spec/20_ALGORITHM_INVARIANTS.md`
- `extra/rvv-vla/spec/21_PERFORMANCE_HYPOTHESES.md`

Correct VLA semantics and parser behavior are invariants. LMUL choice,
compaction details, escape propagation strategy, packed-control thresholds and
related microarchitecture remain tunable unless explicitly locked by the
specification.

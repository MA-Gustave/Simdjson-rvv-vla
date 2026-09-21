# Runtime Dispatch, Build Model and Amalgamation

## 1. Frozen-baseline identities

At v4.6.11 the reserved slots are:

```text
SIMDJSON_IMPLEMENTATION_ID_rvv = 9        // currently commented
instruction_set::RVV = 0x80000            // currently commented
SIMDJSON_IMPLEMENTATION_ID_rvv_vls = 10
instruction_set::RVV_VLS = 0x100000
```

Activate the reserved `rvv` identities only after verifying they are still free in the frozen/rebased target.

## 2. Compile-time macros

Expected backend controls include `SIMDJSON_IMPLEMENTATION_RVV` and `SIMDJSON_CAN_ALWAYS_RUN_RVV`, plus a target-region convenience macro only if needed. Reuse existing `SIMDJSON_IS_RISCV64`, `SIMDJSON_HAS_RVV_INTRINSICS`, and `SIMDJSON_IS_RVV` semantics instead of redefining them.

## 3. Runtime hardware detection

On Linux prefer standard OS-visible capability information:

1. `riscv_hwprobe` when headers/syscall definitions are available;
2. standard ELF HWCAP/getauxval mechanisms when the relevant V bit is available in headers;
3. compile-time always-RVV mode only when the whole binary/build contract guarantees V.

Guard optional headers/macros. Never infer runtime support solely because the compiler exposes RVV intrinsics. Never probe by executing RVV and catching SIGILL.

## 4. Compilation isolation modes

### Mode A — separate RVV translation unit/object (conservative baseline)

Compile backend implementation code with the required RVV ISA while the rest of the library remains baseline RISC-V. Cross-boundary signatures use only ordinary scalar/pointer/parser types. Runtime dispatch prevents calls on unsupported hardware.

This is the default engineering assumption until a simpler mixed-ISA mode is proven.

### Mode B — per-function/region target attributes

Current GCC documents RISC-V `target("arch=...")` and GCC target pragmas, including extension enablement forms such as `arch=+v`. Before relying on this mode, compile and run a minimal probe using the exact RVV intrinsics and compiler version.

Do not assume Clang has equivalent behavior or spelling. Probe it independently.

### Mode C — global RVV build

When the complete application is intentionally built for RVV, `SIMDJSON_CAN_ALWAYS_RUN_RVV` can bypass runtime dispatch. This mode is useful for controlled benchmarks but does not replace the generic runtime-dispatch product goal.

## 5. ABI boundary

Sizeless RVV types may not appear in functions crossing from baseline code to RVV code. Keep them implementation-local.

## 6. Build integration areas

Inspect the actual target tree before editing:

```text
CMakeLists.txt
include/simdjson/implementation_detection.h
include/simdjson/portability.h
include/simdjson/internal/instruction_set.h
include/simdjson/builtin/implementation.h
src/internal/isadetection.h
src/implementation.cpp
src/simdjson.cpp
singleheader/amalgamate.py
```

Add only files needed by the current architecture. Old patches are historical guidance, not apply-ready diffs.

## 7. Selection and priority

`SIMDJSON_FORCE_IMPLEMENTATION=rvv` must work according to existing force-selection semantics.

Do not assume the automatic dispatcher should universally prefer VLA over VLS. `rvv_vls` often exists only in fixed-VLEN/global-target builds, while `rvv` targets generic scalable deployment. If both are selectable in one supported build mode, default preference requires benchmark evidence and an ADR.

## 8. Single-header/amalgamation

Represent the backend correctly in the amalgamation generator. Single-header support may end in one of these documented states:

- **Full mixed-ISA runtime dispatch** — only after GCC/Clang modes are proven;
- **Global-RVV only** — the TU must be compiled with V enabled;
- **Temporarily unsupported for generic dispatch** — normal library supports it through separate object/TU isolation.

Accuracy is more important than artificial parity.

## 9. Non-RISC-V isolation

x86, Arm, PPC and LoongArch builds must not include RISC-V headers or options. Follow existing conditional include/amalgamation conventions.

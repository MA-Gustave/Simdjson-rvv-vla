# CI, Toolchains and Hardware

## 1. CI purpose

CI proves build isolation and correctness across vector lengths/toolchains. It does not prove performance.

## 2. Correctness matrix

Required QEMU VLEN matrix:

```text
128
256
512
1024
```

Use one shared CPU/config helper so all workflows exercise the same intended RVV 1.0 semantics.

## 3. Toolchain policy

Validate current GCC and Clang cross toolchains that support the required RVV 1.0 intrinsic API. Once a combination is known-good, pin CI versions for reproducibility while maintaining at least one newer-toolchain lane when practical.

Do not freeze obsolete versions merely because the January branch used them.

## 4. Compile-probe suite

Before large backend work, add tiny probes for:

- selected RVV intrinsic names/signatures;
- sizeless-type restrictions relied on by the design;
- m1/m2 candidate lane/index conversions;
- `vcompress`, `vid`, `vfirst`, `vcpop`, slides/gathers used by candidate kernels;
- GCC per-function/region `target("arch=+v")` behavior;
- Clang equivalent behavior if claimed;
- separate-TU RVV object linkage into a baseline RISC-V library.

A failed probe is a design input, not something for the agent to guess around.

## 5. CI tiers

Suggested tiers:

- **smoke:** build + focused RVV kernel tests on one VLEN/compiler;
- **full:** GCC/Clang, all four VLENs, relevant CTest;
- **nightly:** fuzz smoke, sanitizers where meaningful, corpus/equivalence suites, amalgamation modes.

## 6. Non-RISC-V protection

Keep at least representative x86/Arm host builds to prove conditional includes/options do not leak RVV requirements.

## 7. Real hardware

Final benchmarking requires a standard RVV 1.0 Linux machine with known VLEN and usable performance counters where possible. Record exact CPU/board/server, firmware, kernel, memory and VLEN.

A one-month rented dedicated K-series/other RVV machine is sufficient for the final campaign if it is stable enough; provider choice is not architectural.

## 8. RepoDiag integration

RepoDiag may orchestrate declared configure/build/CTest/fuzz validation once those commands are stable. It does not replace simdjson tests and must not turn QEMU timing into a performance gate.

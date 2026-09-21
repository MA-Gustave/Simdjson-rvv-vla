# Baseline and Upstream Context

## 1. Frozen development baseline

Development starts from simdjson **v4.6.11** in the clean `simdjson-upstream` clone. The January `feature/rvv-backend` tree is read-only reference material.

The baseline is frozen to avoid mixing algorithm development with moving-target integration work. Rebase checkpoints are mandatory and defined in `22_REBASE_POLICY.md`.

## 2. Upstream RVV state at the freeze date

v4.6.11 contains the fixed-vector-length backend `rvv_vls` with `include/simdjson/rvv-vls/*` and `src/rvv-vls.cpp`. It uses the generic Stage 1 infrastructure and fixed-vector-length RVV types.

The reserved identities for a scalable backend still exist as commented slots in v4.6.11:

```text
SIMDJSON_IMPLEMENTATION_ID_rvv -> 9   (commented/reserved)
instruction_set::RVV          -> 0x80000 (commented/reserved)
SIMDJSON_IMPLEMENTATION_ID_rvv_vls -> 10
instruction_set::RVV_VLS           -> 0x100000
```

Do not assume those reservations can never change after rebasing; re-check them at each checkpoint.

## 3. Public design discussion

The public RVV work already identified two architectures:

- VLS: fixed vector length, fits the generic backend abstraction more naturally;
- VLA: custom scalable backend, potentially more instruction-level parallelism and fewer fixed 64-bit-mask constraints.

PR #2593 explicitly described a future fully scalable VLA `rvv` backend with runtime dispatch as a separate body of work. As of the 2026-09-19 verification, no public full VLA simdjson backend had superseded that plan.

PR #2617 demonstrates that upstream accepts a localized VLA-style RVV path using dynamic `vsetvl` for a string-builder helper. It does not constitute the full parser backend.

## 4. Exact generic semantics remain authoritative

For Stage 1, the frozen source in `src/generic/stage1/*` is the correctness oracle. Important observations from v4.6.11 include:

- structural indexes are `uint32_t`;
- generic Stage 1 tracks string/escape state across fixed blocks;
- pseudo-structurals include operator starts and scalar starts;
- streaming finalization, sentinels and partial-document logic live in the generic structural indexer;
- UTF-8 errors and unescaped control characters participate in Stage 1 completion semantics.

The VLA implementation may restructure the hot loop but must preserve those externally observable contracts.

## 5. Current toolchain fact versus open question

Current GCC documentation exposes RISC-V per-function target options through `__attribute__((target("arch=...")))` / `#pragma GCC target`, including extension enablement such as `arch=+v`. This makes mixed-ISA compilation technically plausible.

However, the project must **compile-probe the exact compiler versions and the exact RVV intrinsic region** before choosing per-function attributes as the portable build mechanism. Clang parity must not be assumed from GCC documentation. Separate-TU/object compilation remains the conservative build path.

## 6. Old branch value

The old branch contains extensive RVV-specific tests, tools, QEMU scripts, CI, fuzzers, benchmarks and documentation. These assets are valuable. Its old core architecture is not canonical: fixed semantic 64-byte blocks, LMUL=m1 locks, scalar mask export, incomplete optimization and known bring-up defects must not be copied blindly.

## 7. Upstream monitoring rule

Before each rebase checkpoint, search current simdjson issues/PRs/discussions for:

```text
rvv
rvv-vla
vector length agnostic
RISC-V Vector
runtime dispatch
```

If a public competing VLA implementation appears, compare designs before continuing. Do not duplicate upstream blindly.

## 8. Key references

See `18_REFERENCE_PATHS.md` for exact source paths and external references.

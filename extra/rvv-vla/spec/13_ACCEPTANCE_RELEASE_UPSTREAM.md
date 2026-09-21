# Acceptance, Release and Upstreaming

## 1. Definition of done

Product-complete means correctness, build/dispatch, performance-evidence, documentation and rebase gates all pass.

## 2. Correctness gates

- focused RVV tests pass;
- relevant CTest passes in supported GCC/Clang modes;
- QEMU VLEN 128/256/512/1024 matrix passes;
- DOM/On-Demand/UTF-8/minify/streaming differential suites pass;
- fuzz smoke/campaign gates pass;
- non-RISC-V CI shows no regression.

## 3. Build/dispatch gates

- `rvv` registration/force-selection works;
- runtime detection is safe in claimed modes;
- baseline RISC-V binary does not execute RVV on unsupported hardware;
- no sizeless ABI leaks;
- `rvv_vls` remains buildable/reference-capable;
- every advertised single-header mode is actually tested.

## 4. Performance-hypothesis gates

Before finalizing defaults, every promoted H-ID must have an evidence entry in `14_DECISION_LOG.md`/`24_PROJECT_STATE.md` showing tested candidates, hardware, corpus, result and rationale.

Unpromoted hypotheses must not be described as final architecture in docs or comments.

## 5. Performance publication gate

- real hardware A/B data exists;
- `rvv_vls` measured on same target when technically possible;
- raw data/metadata preserved;
- Stage 1 and end-to-end results both reported;
- chosen algorithm has at least one ablation showing source of gain/loss;
- regressions are disclosed;
- no QEMU timing is presented as speed evidence.

## 6. Rebase gate

Before final upstream preparation, execute the final checkpoint in `22_REBASE_POLICY.md`, resolve upstream conflicts separately from algorithm changes, and rerun acceptance tests/benchmarks where code generation or hot paths changed.

## 7. Documentation gate

Document build/selection, VLA-vs-VLS distinction, single-header support, benchmark method, current accepted algorithm choices, provenance, and significant AI assistance.

## 8. Suggested commit series

A reviewable series may separate registration/runtime detection, Stage 1 core/tests, utility/Stage2 integration, CI/tooling, and docs/benchmarks. Do not artificially split tightly coupled correctness code or submit one opaque mega-diff.

## 9. PR description

Include problem, architecture, supported toolchains/build modes, correctness matrix, hardware/VLEN, benchmark methodology and raw artifacts, before/after data versus `rvv_vls`, algorithm ablations, limitations and AI-use process disclosure.

## 10. Claims

Use scoped language such as "X% faster on workload/corpus Y on CPU Z". Avoid universal fastest/always-faster claims without broad evidence.

## 11. Negative-result handling

If final performance does not justify a complete backend, keep the research branch/report and consider independently useful subcomponents (tests, runtime detection, writer optimization) only if they stand on their own evidence.

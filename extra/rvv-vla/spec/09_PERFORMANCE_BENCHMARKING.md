# Performance Benchmark Protocol

## 1. Scientific rule

Only real RVV hardware can support speed claims. QEMU is a correctness/coverage tool.

A/B comparisons keep source revision, compiler, optimization flags, corpus, CPU affinity, machine state and frequency policy constant. The algorithm/backend variant is the independent variable.

## 2. Reference backends

Always measure `fallback`, `rvv_vls` when technically buildable for the target, and `rvv`. Do not silently omit a competitor.

## 3. Candidate-level benchmarking before architecture lock

The project intentionally keeps several performance hypotheses open. Before promoting a hypothesis, benchmark the relevant candidates using the matrix in `23_BENCHMARK_EXPERIMENT_MATRIX.md`.

At minimum evaluate:

- Stage 1 byte LMUL m1 vs m2 on the primary hardware;
- sparse/event vs vector-compaction structural writer and an adaptive policy;
- rare-event vs dense-vector escape path where both are implemented;
- quote-prefix candidate(s) if alternatives are practical;
- UTF-8 schedule U1/U2 and U3 reference if needed;
- minify compaction candidate against a reference alternative where practical.

## 4. Benchmark layers

### Micro

- classification/string-state microkernels as needed;
- structural-index emission at controlled densities;
- Stage 1 only;
- UTF-8 only;
- minify only.

### End-to-end

- DOM parse;
- On-Demand benchmark workloads;
- representative query workloads available in simdjson.

A microbenchmark winner does not automatically become the shipping choice if end-to-end behavior disagrees.

## 5. Corpus classes

Use real simdjson corpora plus deterministic synthetic sets covering dense arrays, large strings, structurally sparse data, quote-heavy strings, escape-heavy strings, ASCII-heavy and UTF-8-heavy data. Record structural/backslash/quote density for synthetic cases.

## 6. Metrics

Primary: throughput with unit stated, cycles/byte, instructions/byte when reliable. Secondary: branch/cache misses, wall time, structural density, backslash/quote density and observed CPU frequency.

## 7. Sampling and statistics

For important claims:

- warm up first;
- use randomized or alternating A/B order to reduce thermal/frequency drift;
- collect at least 10 independent samples, preferably 20+ for close results;
- report median plus spread (IQR or percentile range);
- for aggregate corpus claims, report geometric mean of per-workload speed ratios;
- when practical, report a bootstrap 95% confidence interval for the aggregate/median difference;
- never select only the fastest run.

Predefine any outlier/throttling rejection rule before examining which backend benefits.

## 8. CPU controls

Pin to a stable core/CPU set, use the most stable available governor/frequency mode, monitor thermal throttling, minimize background services, and record CPU model, VLEN, memory, kernel/firmware and compiler.

For rented hardware, prefer dedicated/bare-metal allocation. Shared/noisy instances are acceptable for exploratory tuning but weak evidence for final publication.

## 9. Build controls

Use Release/`-O3` according to upstream practice. Keep compiler version, LTO, thread/exception configuration, datasets and nonessential ISA flags equal. `rvv_vls` gets the exact fixed-VLEN flags required by the target; `rvv` must not require a fixed-vector-bits flag.

## 10. Density analysis

For structural writer experiments, sweep controlled structural density from sparse to dense. For escape algorithms, sweep backslash-run density/length. For string-state work, sweep quote density. This determines whether an adaptive path has a real crossover point.

## 11. Ablation requirements

The final performance report should include at least:

- chosen writer vs reference writer;
- chosen Stage 1 vector shape vs the strongest rejected candidate;
- normal JSON vs escape-heavy JSON;
- chosen UTF-8 schedule vs at least one reference if UTF-8 scheduling materially affects the result.

## 12. Promotion rule for a hypothesis

A performance hypothesis becomes an accepted default only if:

1. correctness is identical;
2. generated code has no unexplained pathological spills/ISA requirements;
3. primary-hardware data is repeatable;
4. it wins or materially simplifies without a meaningful loss over representative workloads;
5. the result is recorded in `14_DECISION_LOG.md` and `24_PROJECT_STATE.md`.

A target-specific adaptive branch may be chosen if it has a clear crossover and the branch overhead is itself measured.

## 13. Project gates

Continue tuning when Stage 1 shows a consistent 5-10% improvement or a strong scalable-deployment advantage. The publishable target remains roughly 15% geometric-mean Stage 1 gain against `rvv_vls` or an equivalent end-to-end result. Revisit the architecture if mature Stage 1 remains >10% slower on ordinary JSON.

## 14. Result artifacts

Save exact git commit/spec version, metadata, raw CSV, compiler version, kernel/CPU/VLEN data, build flags, summary tables, plots generated from raw data, and experiment/hypothesis IDs. Never publish only screenshots.

# Sources

## simdjson

- simdjson repository  
  https://github.com/simdjson/simdjson

- PR #2593 — `rvv_vls` backend by Olaf Bernstein / `camel-cdr`  
  https://github.com/simdjson/simdjson/pull/2593

- RVV optimization proposal / historical context  
  https://github.com/simdjson/simdjson/issues/2423

## Olaf Bernstein / camel-cdr RVV work

- `rvv-bench` benchmark source  
  https://github.com/camel-cdr/rvv-bench

- RVV benchmark results database  
  https://camel-cdr.github.io/rvv-bench-results/

- SpacemiT X60 / K1 instruction and algorithm measurements  
  https://camel-cdr.github.io/rvv-bench-results/spacemit_x60/index.html

- Vectorizing Unicode conversions on real RISC-V hardware  
  https://camel-cdr.github.io/rvv-bench-results/articles/vector-utf.html

- RISC-V Vector Extension for Integer Workloads: An Informal Gap Analysis  
  https://gist.github.com/camel-cdr/99a41367d6529f390d25e36ca3e4b626

- SWAR UTF-8 Validation with `xperm4`  
  https://camel-cdr.github.io/rvv-bench-results/articles/xperm4-utf8-validation.html

## Current project

- MA-Gustave / Simdjson-rvv-vla  
  https://github.com/MA-Gustave/Simdjson-rvv-vla

Relevant current files:

```text
src/rvv/stage1.h
src/rvv/string_scanner.h
src/rvv/utf8_validation.h
src/rvv/minify.h
include/simdjson/rvv/intrinsics.h
scripts/rvv/perf/
extra/rvv-vla/spec/21_PERFORMANCE_HYPOTHESES.md
extra/rvv-vla/spec/23_BENCHMARK_EXPERIMENT_MATRIX.md
```

## Interpretation note

Published instruction timings are used to identify high-risk operations and design
experiments. They are not substitutes for end-to-end simdjson measurements.

The optimization plan should be revised after the first native baseline and after every
major architecture experiment.

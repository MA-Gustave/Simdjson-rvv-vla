# Benchmark and Promotion Protocol

## 1. Rule zero

No optimization is "faster" until measured on physical RVV hardware.

QEMU is excluded from performance claims.

## 2. Preserve a clean baseline

Before the first optimization commit:

```text
current VLA commit
upstream baseline commit/tag
compiler versions
flags
hardware metadata
corpus hashes
raw benchmark output
```

Store these with the results.

## 3. Required measurement layers

Every major optimization should be evaluated at three layers.

### Layer A — microbenchmark

Measure the mechanism itself:

- mask extraction;
- quote prefix;
- escape run;
- structural writer;
- classification.

### Layer B — Stage 1

Measure bytes/s and cycles/byte on representative corpora.

### Layer C — end to end

Measure:

- DOM full parse;
- On-Demand where supported;
- minify;
- standalone UTF-8.

A change that wins Layer A and loses Layer C is rejected or redesigned.

## 4. Trial protocol

Use:

- fixed/pinned CPU when possible;
- warmup;
- alternating candidate order;
- enough repetitions for stable median;
- raw per-trial output;
- median plus dispersion.

A VPS can be useful for relative comparisons but noisy-neighbor effects must be reported.

## 5. Corpus categories

Use both real and synthetic data.

Real:

- `twitter.json`;
- `citm_catalog.json`;
- additional standard simdjson benchmark corpora if available.

Synthetic:

- structural-density sweep;
- quote-density sweep;
- backslash-density/run sweep;
- ASCII and multilingual UTF-8;
- whitespace-density sweep.

Synthetic cases explain behavior. Real corpora decide relevance.

## 6. Compiler matrix

For primary decisions:

```text
GCC Release/O3
Clang Release/O3
```

Do not mix compilers in a direct speedup ratio.

A candidate that only wins because one compiler generates poor baseline code should be
documented as compiler-specific.

## 7. Counters

Where available collect:

```text
cycles
instructions
branches
branch misses
L1 misses
LLC misses
```

On RVV hardware, instruction counts alone can be misleading because one vector instruction
may have very different throughput at different LMUL.

## 8. Assembly audit

For each hot-loop candidate save disassembly and count:

```text
vsetvli/vsetivli
vcompress
vrgather
viota
vslide*
vfirst
vcpop
vector loads/stores
scalar loads/stores
```

Also inspect for spills.

## 9. Promotion threshold

Do not impose a universal percentage threshold.

Promotion requires:

- statistically stable improvement on the intended workload;
- no important correctness regression;
- no large unexplained regression on representative corpora;
- no unacceptable code complexity;
- acceptable results with both major compilers;
- portability story understood.

Very small wins below measurement noise are not sufficient.

## 10. Record format

For every candidate:

```text
EXPERIMENT_ID:
GIT_COMMIT:
BASELINE_COMMIT:
HARDWARE:
VLEN:
COMPILER:
FLAGS:
ALGORITHM:
CORPUS:
TRIALS:
MEDIAN:
MAD_OR_SPREAD:
CYCLES_PER_BYTE:
INSTRUCTIONS_PER_BYTE:
ASSEMBLY_NOTES:
REGRESSIONS:
CONCLUSION:
PROMOTE: yes/no
RAW_RESULTS:
```

## 11. Publication standard

Any public result intended for simdjson maintainers should include:

- exact repository commit;
- exact upstream comparison;
- hardware;
- compiler and flags;
- raw data;
- reproducible command;
- median and variability;
- disclosure of VPS/shared-host status if relevant.

A reproducible +8% is more credible than an unexplained screenshot showing +25%.

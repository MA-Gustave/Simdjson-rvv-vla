# Benchmark Experiment Matrix

This is the planned evidence matrix for WP-13. It prevents ad-hoc benchmark cherry-picking.

## E-MICRO-001 — Structural writer density sweep

Variables:

```text
writer: W1, W2, W3
LMUL: L1, L2
structural density: 0%, 0.5%, 1%, 2%, 5%, 10%, 20%, 40%, 70%
```

Output: bytes/s, cycles/byte, instructions/byte, crossover density, branch cost.

## E-MICRO-002 — Escape density/run sweep

Variables:

```text
escape path: E1, E2, EA
backslash-event density: 0%, 0.1%, 0.5%, 1%, 5%, 20%, 50%
run lengths: 1,2,3,4,8,16, boundary-crossing mixes
```

Always include the E0 zero-backslash case.

## E-MICRO-003 — Quote parity/classification

Compare Q/classification candidates on quote-sparse, string-heavy and structural-heavy deterministic inputs. Only keep multiple candidates if the difference is above measurement noise or materially changes register pressure.

## E-MICRO-004 — UTF-8 standalone

Datasets: ASCII, valid 2/3/4-byte mixes, invalid patterns, boundary-heavy. Compare standalone LMUL candidates.

## E-STAGE1-001 — Stage 1 algorithm matrix

Start with a factorial subset, not every theoretical combination:

```text
L1+W1+E1+Q1
L2+W1+E1+Q1
best-L+W2/E adaptive candidate
best-L+best-W+E2 on escape-heavy
best structural/string candidate + U1
same + U2
same + U3 reference if required
```

Use representative real corpora plus deterministic synthetic datasets.

## E-E2E-001 — Parser end-to-end

Compare fallback, `rvv_vls`, and final `rvv` for DOM and On-Demand. Randomize/alternate backend run order. Record corpus-level geometric mean and per-workload regressions.

## E-MIN-001 — Minify

Compare M1 and any retained M2/reference on whitespace-heavy, string-whitespace-heavy, escape-heavy and ordinary corpora.

## E-PORT-001 — Cross-hardware confirmation

On a second RVV machine/generation, rerun the final selected backend versus `rvv_vls` where buildable. Do not retune thresholds first; test portability of the primary decision. Retuning can be a separate experiment.

## Experiment result template

```text
EXPERIMENT_ID:
SPEC_VERSION:
GIT_COMMIT:
HARDWARE:
VLEN:
KERNEL/FIRMWARE:
COMPILER:
FLAGS:
CANDIDATES:
CORPUS:
SAMPLES:
ORDER_RANDOMIZATION:
THERMAL/FREQUENCY_NOTES:
MEDIANS:
SPREAD/CI:
COUNTERS:
ASSEMBLY_NOTES:
CONCLUSION:
PROMOTION_RECOMMENDATION:
RAW_ARTIFACT_PATH:
```

# LMUL, VLEN and Performance Portability

## 1. VLA does not mean "always use the largest LMUL"

RVV LMUL provides two major benefits:

- more lanes per loop iteration;
- natural mixed-width relationships.

But LMUL also:

- reduces the number of independent vector register groups;
- increases register pressure;
- can make permutation instructions much more expensive;
- can increase masked-operation cost on some microarchitectures.

The correct LMUL is algorithm- and hardware-dependent.

## 2. Current Stage 1 choice

Current Stage 1 uses:

```text
bytes: e8m2
indexes: e32m8
```

This gives equal lane counts.

The design is convenient but makes output compaction a high-LMUL operation.

The performance redesign should make LMUL a byte-processing choice rather than an index
writer constraint.

## 3. Proposed candidates

### L1 — e8m1 with manual unrolling

Advantages:

- cheap permutation operations;
- many available register groups;
- possibly easier compiler scheduling.

Disadvantages:

- more loop/control overhead;
- fewer bytes per vector instruction.

### L2 — e8m2

Current default and likely strong balanced candidate.

On X60 VLEN=256:

```text
64 byte lanes
```

### L4 — e8m4

Potentially attractive after the control plane no longer widens into m8 permutation
graphs.

On X60 VLEN=256:

```text
128 byte lanes
```

Do not test L4 fairly until the structural writer and string control no longer depend on
high-LMUL gathers/compresses.

## 4. VLEN scaling expectation

A good VLA algorithm should gain from larger VLEN without making its hardest operations
superlinearly more expensive.

Ideal scaling:

```text
wider VLEN
 -> more cheap byte compares per loop
 -> more mask bits
 -> control processed in fixed-size words
```

Risky scaling:

```text
wider VLEN
 -> larger LMUL permutation
 -> increasingly expensive all-to-all gather/compress
```

## 5. Multiple 64-bit control words

Do not limit VLA to <=64 lanes.

For VLEN 512/1024 and/or LMUL>1, process:

```text
word[0]
word[1]
...
```

with explicit carry state.

The number of control words grows linearly with processed bytes, but each word uses cheap
integer operations.

## 6. Cross-hardware experiment

For every final candidate record:

```text
CPU
VLEN
DLEN if known
compiler
LMUL
bytes/iteration
cycles/byte
GB/s
vsetvli count
permutation count
```

At minimum compare:

- X60/K1;
- one second RVV 1.0 implementation when accessible.

The public rvv-bench database currently includes several newer RVV 1.0 implementations,
including SpacemiT K3 cores, which are valuable references even before direct access.

## 7. Hardware-specific thresholds

Adaptive thresholds such as structural-density crossover may differ by CPU.

Preferred order:

1. find a threshold that is robust across machines;
2. if impossible, consider a small microarchitecture-specific policy;
3. avoid a large runtime-tuning framework unless the gains justify maintenance.

## 8. Runtime VLEN specialization without fixed-VLEN code

VLA code may still choose algorithms based on runtime `vlenb`/VLMAX.

Example concept:

```text
if native byte capacity <= X:
    use path A
else:
    use path B
```

This preserves one VLA binary while allowing algorithmic adaptation.

Only add such specialization after cross-VLEN measurement shows a real need.

# simdjson RVV native performance lab

This harness compares the current RVV-VLA backend (`rvv`) against the pristine
simdjson v4.6.11 RVV-VLS backend (`rvv_vls`) on **native RISC-V RVV hardware**.

It intentionally refuses non-riscv64 hosts and QEMU/TCG-style emulation by
default. A virtual machine with hardware execution is allowed but reported as a
warning because frequency scheduling/noisy neighbours can increase variance.

## What is compared

The harness builds:

- `current_vla`: this worktree, scalable RVV, compiled with `-march=rv64gcv`.
- `current_fallback`: scalar fallback from the same binary (control).
- `upstream_vls_128`, `upstream_vls_256`, `upstream_vls_512`: pristine
  simdjson v4.6.11 / f5de14f09256982933af2849beb43778bd421ca7,
  compiled at each fixed VLEN supported by the machine.
- `upstream_fallback`: scalar fallback from the upstream build (control).

For optimization attribution, `--vla-variants phase1` additionally builds
isolated RVV-VLA candidates from the same worktree:

- `vla_legacy`: packed control disabled, sparse writer reduced to the legacy
  one-event behavior.
- `vla_sparse_index`: sparse structural writer only.
- `vla_packed_index`: structural `vsm.v` + scalar bit enumeration only.
- `vla_sparse_escape`: sparse backslash-event path only.
- `vla_packed_escape`: packed backslash algebra only.
- `vla_packed_quote`: packed 64-bit quote prefix XOR only.
- `vla_packed_shift`: packed scalar-start shift only.

`current_vla` is always included as the fully composed candidate. This keeps
the normal upstream comparison intact while making individual wins/regressions
visible in the generated report.

VLS variants larger than the hardware VLEN are skipped automatically.

Measured kernels:

- DOM full parse (`dom_all`) using simdjson's `benchmark/dom/parse`.
- Stage 1 (`stage1`) using the same benchmark.
- Stage 2 (`stage2`) as a sanity/control metric.
- `minify` using a no-allocation timed microbenchmark.
- `utf8` validation using a no-allocation timed microbenchmark.

The generated corpus is deterministic and includes structurally dense JSON,
numbers, long ASCII strings, UTF-8 strings, and escaped strings. `twitter.json`
and `citm_catalog.json` from the current tree are copied when available.

## VPS requirements

Before buying/renting a VPS, ask the provider to confirm:

- `uname -m` is `riscv64`.
- The guest exposes RISC-V Vector Extension **V 1.0**, not an emulated CPU.
- VLEN is at least 128 bits; 256/512/1024 are more interesting for this project.
- Debian 13 or another distro with GCC 14+ or Clang 19+ is preferred.
- At least 4 vCPUs, 4 GiB RAM, and 10 GiB free disk.
- Dedicated/pinned vCPU is preferable to heavily oversubscribed shared vCPU.
- `perf_event` access is useful but optional.

Do not accept a "RISC-V VPS" that is actually QEMU/TCG emulation for performance
claims. QEMU remains useful only for correctness.

## Debian 13 packages

```bash
sudo apt update
sudo apt install -y git cmake ninja-build build-essential python3 file
```

For the tested GCC path, GCC 14+ is expected.

## One-command standard run

From the **current RVV-VLA repository root**:

```bash
python3 scripts/rvv/perf/bench_native.py all
```

Default workspace:

```text
../rvv-perf-lab
```

The command:

1. probes the native machine and measures hardware VLEN;
2. clones/checks out pristine simdjson v4.6.11;
3. generates the deterministic corpus;
4. builds current RVV-VLA + fallback;
5. builds all usable upstream RVV-VLS fixed-VLEN variants;
6. runs alternating trials pinned to one CPU;
7. writes raw CSV, summary CSV, metadata and a Markdown report.

## Profiles

Fast smoke test:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile quick
```

Normal baseline:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile standard
```

Phase-1 A/B run (recommended before changing defaults):

```bash
python3 scripts/rvv/perf/bench_native.py all --profile standard --vla-variants phase1
```

You can also select individual variants, for example:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile quick \
  --vla-variants legacy,packed-index,packed-quote
```

Longer run for results you may want to publish:

```bash
python3 scripts/rvv/perf/bench_native.py all --profile publication
```

You can choose the CPU:

```bash
python3 scripts/rvv/perf/bench_native.py all --core 2
```

And the compiler:

```bash
python3 scripts/rvv/perf/bench_native.py all --compiler gcc
python3 scripts/rvv/perf/bench_native.py all --compiler clang
```

## Useful partial commands

Machine only:

```bash
python3 scripts/rvv/perf/bench_native.py preflight
```

Prepare/build only:

```bash
python3 scripts/rvv/perf/bench_native.py prepare
```

Benchmark already-built binaries:

```bash
python3 scripts/rvv/perf/bench_native.py run
```

Regenerate report from raw CSV:

```bash
python3 scripts/rvv/perf/bench_native.py report
```

## Output

A run writes files like:

```text
../rvv-perf-lab/
  metadata.json
  lscpu.txt
  cpuinfo.txt
  current_git_status.txt
  current_git_diff.patch
  corpus/
  upstream-v4.6.11/
  builds/
  bin/
  results/raw.csv
  results/summary.csv
  results/report.md
```

The report compares RVV-VLA against the **best upstream RVV-VLS variant actually
measured on that machine**, while preserving each 128/256/512 result separately.

## Interpretation rules

- Use throughput ratios from the same machine/run, not QEMU numbers.
- Treat a VPS as a controlled comparative environment, not bare metal.
- Prefer results where per-trial MAD/median is below ~3%.
- If noise is high, increase trials and use a less-loaded/dedicated CPU.
- Do not mix compiler families or optimization flags in one speedup claim.
- Keep the raw CSV and metadata with any published result.

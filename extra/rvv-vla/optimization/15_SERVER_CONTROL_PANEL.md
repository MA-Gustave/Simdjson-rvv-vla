# Native RVV Server Control Panel

## Purpose

`scripts/rvv/server_control.py` is the operator-facing entry point for a real
RISC-V server or VPS. It is intentionally an orchestration layer only: it does
not duplicate correctness or benchmark logic.

Under the panel:

- `scripts/rvv/native_suite.py` owns qualification, correctness orchestration,
  benchmark campaigns, summaries and native result bundles;
- `scripts/rvv/run_tests.py` owns RVV correctness and acceptance execution;
- `scripts/rvv/perf/bench_native.py` owns native RVV/VLEN preflight and
  performance measurement.

This separation keeps one source of truth for every test and makes the panel
safe to extend without silently changing benchmark methodology.

## Interactive mode

From the repository root:

```bash
python3 scripts/rvv/server_control.py
```

The terminal menu provides:

1. machine/repository doctor;
2. Debian/Ubuntu prerequisite installation;
3. safe Git fast-forward synchronization on a clean worktree;
4. native RVV/VLEN preflight;
5. smoke campaign;
6. phase-1 A/B campaign;
7. compiler/runtime matrix;
8. standard benchmark campaign;
9. publication benchmark campaign;
10. safe automatic first-run workflow;
11. latest native-suite report display.

Every action creates a timestamped control session and keeps its own log.

## Safe automatic first run

For a new server, the preferred unattended command is:

```bash
python3 scripts/rvv/server_control.py --auto
```

The automatic policy is deliberately gated:

```text
doctor
  -> compiled native RVV/VLEN preflight
     -> smoke
        -> phase1
```

The sequence stops at the first blocking failure. A failed or emulated host
therefore does not continue into an expensive performance campaign.

`--auto` does not run `git pull` and does not install packages. Repository
mutation and package installation remain explicit operator actions so the test
commit stays reproducible.

## Scripted actions

Every menu action is also directly callable:

```bash
python3 scripts/rvv/server_control.py doctor
python3 scripts/rvv/server_control.py preflight
python3 scripts/rvv/server_control.py smoke
python3 scripts/rvv/server_control.py phase1
python3 scripts/rvv/server_control.py matrix
python3 scripts/rvv/server_control.py standard
python3 scripts/rvv/server_control.py publication
python3 scripts/rvv/server_control.py latest
```

The `publication` action uses the full `standard` correctness/acceptance matrix
and overrides the performance profile to `publication`.

## Server setup

On Debian/Ubuntu, prerequisites can be installed through the panel:

```bash
python3 scripts/rvv/server_control.py setup --yes
```

Add Clang to the package request with:

```bash
python3 scripts/rvv/server_control.py setup --yes --with-clang
```

The panel installs only general build/runtime prerequisites. It does not change
CPU governors, kernel parameters, SSH configuration or provider settings.

## Repository synchronization

A controlled fast-forward-only update is available:

```bash
python3 scripts/rvv/server_control.py sync
```

The action refuses to pull when the worktree is dirty or when no upstream is
configured. Automatic test mode never synchronizes implicitly.

## Performance controls

Pin benchmark execution to a CPU:

```bash
python3 scripts/rvv/server_control.py phase1 --core 2
```

Limit upstream VLS widths:

```bash
python3 scripts/rvv/server_control.py phase1 --vls-bits 128,256
```

Override the VLA A/B candidate set passed to `native_suite.py`:

```bash
python3 scripts/rvv/server_control.py phase1 \
  --vla-variants legacy,packed-index,packed-quote
```

Control parallel build jobs with `--jobs`.

## Doctor policy

The lightweight doctor records architecture, virtualization, QEMU/TCG hints,
CPU affinity, RAM, disk, Git provenance, GCC/Clang, CMake, Ninja, `taskset` and
`perf` availability.

It treats these conditions as blocking for a native performance campaign:

- architecture is not `riscv64`;
- QEMU/TCG-like execution is detected;
- mandatory build tools are missing.

A virtualized but hardware-backed RISC-V guest is reported as a warning rather
than blocked. The authoritative RVV capability check remains the compiled VLEN
probe in `bench_native.py preflight`.

## Results

Default control sessions are written below:

```text
../rvv-server-runs/control-<UTC>/
```

A session contains:

```text
control-summary.md
control-summary.json
control.log
doctor.txt
doctor.json
<action>.log
<child native-suite directories>/
rvv-server-control-results.tar.gz
```

Each child `native_suite.py` campaign still creates its own
`rvv-native-results.tar.gz` containing the detailed benchmark and correctness
artifacts.

`rvv-server-control-results.tar.gz` is the preferred single file to share after
an interactive or automatic control-panel session. It aggregates control logs,
doctor metadata, preflight metadata and the compact child native-suite bundles,
while excluding large build trees and cloned upstream source trees.

## Recommended campaign order

For the first K1/X60 or equivalent VPS:

```bash
python3 scripts/rvv/server_control.py --auto
```

If the automatic run passes, expand coverage in this order:

```bash
python3 scripts/rvv/server_control.py matrix
python3 scripts/rvv/server_control.py standard
```

Run `publication` only after quick/standard data is stable and noise is
acceptable. QEMU results remain correctness-only regardless of which command is
used.

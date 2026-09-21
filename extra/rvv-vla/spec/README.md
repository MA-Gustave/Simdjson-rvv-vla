# RVV-VLA development specification v1.1

Start with [00_START_HERE.md](00_START_HERE.md).

This directory is the anti-drift implementation specification for a production-grade simdjson RVV 1.0 Vector-Length-Agnostic backend. The frozen development baseline is simdjson `v4.6.11`; upstream `rvv_vls` remains the reference competitor.

Version 1.1 separates **semantic invariants** from **performance hypotheses**. Coding agents must never turn an unmeasured hypothesis into a permanent architecture constraint. Human-readable Markdown is authoritative. `SPEC_MANIFEST.json` is a machine-readable mirror used for agent preflight and automation.

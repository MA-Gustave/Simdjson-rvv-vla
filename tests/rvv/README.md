# RVV-VLA tests

These are correctness tests for the scalable `rvv` backend.

They are opt-in and do not affect unrelated host builds unless
`SIMDJSON_RVV_TESTS=ON` is enabled.

Coverage includes implementation registration, DOM equivalence, Stage 1
boundaries, string-aware minification, UTF-8, number parsing, On-Demand,
streaming, randomized differential parsing, and the bundled JSON corpus.

For the VLEN matrix, use `scripts/rvv/run_tests.py` or the external RepoDiag RVV campaigns. See `doc/rvv-vla-testing.md`.

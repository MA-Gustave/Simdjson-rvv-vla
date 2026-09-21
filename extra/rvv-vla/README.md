# RVV-VLA project area

`spec/` contains the canonical v1.1 design specification.

The production backend deliberately lives in normal simdjson source locations:

- `include/simdjson/rvv.h`
- `include/simdjson/rvv/`
- `src/rvv.cpp`
- `src/rvv/`

Supporting validation lives in:

- `tests/rvv/`
- `scripts/rvv/`
- `cmake/toolchains/`
- `repodiag/`

See the repository-root `RVV_VLA.md` for the complete project map.

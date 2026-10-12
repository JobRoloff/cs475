# Project State: 03-OPQ (SIMD Variant)

## Configuration
- **Variant Name:** `SIMD`
- **Source File:** `src/PolyMult.c` (Clean 2-loop outer product traversal)
- **Optimization Focus:** SIMD Vectorization (`#pragma omp simd`) and instruction-level parallelism.

## Decisions Log
- Dropped software tiling (`PolyMultTiled.c`) because benchmarking in `02-OPQ` showed it introduced heavy loop and boundary-clamping overhead with negligible cache benefits on a single core.
- Transitioning focus to compiler-directed SIMD vectorization on the inner loop.

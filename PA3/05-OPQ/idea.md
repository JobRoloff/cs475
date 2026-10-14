Task: Restructure the tiled polynomial multiplication kernel (`src/PolyMultTiled.c`) to use Local Tile Accumulation, eliminating inner-loop global Read-Modify-Write (RMW) traffic.

### Background & Problem
Currently, the inner loop calls `BPM_ACC` on every iteration, causing every vector FMA (`vfmadd213ps`) to load `c[k]` from global memory, compute, and store `c[k]` right back. This saturates memory bandwidth at ~13 GFLOP/s. 

### Required Changes in `src/PolyMultTiled.c`:

1. **Introduce a Local Tile Scratchpad Buffer:**
   - Inside the tile loop nest, before computing the micro-kernel for a given tile block, allocate a local accumulation buffer on the stack for that tile's output range.
   - The local output length for a tile of degree bounds `(tile_dA, tile_dB)` is `local_len = tile_dA + tile_dB + 1`. 
   - Example: `float c_local[MAX_TILE_OUTPUT_SIZE];` (sized safely to accommodate maximum block dimensions, e.g., `2 * BLOCK_SIZE + 1`).

2. **Zero-Initialize Local Buffer:**
   - Initialize `c_local` elements to `0.0f` for the duration of the tile's computation.

3. **Redirect Inner-Loop Accumulation to Local Memory:**
   - Modify the micro-kernel inner loop so that products are accumulated into `c_local[local_k]` (where `local_k = local_i + local_j`) instead of global `c[k]`.
   - Because `c_local` resides entirely in L1 cache (or CPU registers), the compiler can execute pure vector stores/accumulates without global memory latency stalls or cache-line bouncing.

4. **Global Commit Phase (Tile Writeback):**
   - Once the tile's inner loops finish, perform a single pass to commit the local tile buffer back to the global output array `c`:
     `c[global_k] += c_local[local_k];`
   - This reduces global RMW overhead from $O(N^2)$ individual memory round-trips down to $O(\text{num\_tiles} \times \text{tile\_size})$.

5. **Verification & Testing:**
   - Run `make clean && make all` and execute `./run.sh` to ensure the correctness gate (all 42 oracle cases and 14 gold cases) passes without regression.
   - Run a timed performance sweep to measure the GFLOP/s speedup against the 13 GFLOP/s ceiling.
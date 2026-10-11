# Lab 4 Report: OpenMP Task-Parallel Merge Sort

## 1. Overview & Experimental Setup
This report analyzes the performance and scaling behavior of parallelizing recursive Merge Sort on $N = 100,000,000$ elements using OpenMP tasks across 1 to 8 threads on a quiet fish machine. Execution times were measured using the median of 5 runs via `run_sweep.sh`. Baseline sequential execution (`Merge_sort`) completed in **14.58 seconds** ($T_{\text{baseline}}$). We evaluated four parallel implementations formed by combining two independent design choices: **Task Pruning** (spawning tasks only above a size threshold) vs. **No Pruning**, and **Two-Task Spawning** (spawning a task for both left and right recursive calls) vs. **Single-Task Spawning** (spawning one task for the left branch while the parent thread executes the right branch directly).

---

## 2. Performance Data & Speedup Summary

Speedup is calculated relative to the baseline sequential execution time:

$$\text{Speedup}(p) = \frac{T_{\text{baseline}}}{T(p)}$$

where $T_{\text{baseline}} = 14.58\text{ s}$ and $T(p)$ is the total wall-clock time using $p$ threads.

### Median Runtime Table (Seconds)

| Threads ($p$) | Sequential ($T_{\text{baseline}}$) | No Prune (`NP`) | Pruned (`P`) | Single Task + Pruned (`S`) | Single Task + No Prune (`SNP`) |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | **14.58** | 27.27 | 14.37 | 14.35 | 20.97 |
| **2** | — | 63.83 | 7.48 | 8.01 | 36.11 |
| **3** | — | 76.84 | 7.52 | 6.84 | 47.73 |
| **4** | — | 109.54 | 6.16 | 6.16 | 61.76 |
| **5** | — | 117.04 | 5.83 | 5.48 | 66.90 |
| **6** | — | 133.71 | 4.19 | 5.09 | 74.83 |
| **7** | — | 138.00 | 4.19 | 4.74 | 82.98 |
| **8** | — | 148.78 | **4.10** | **4.38** | 89.40 |

### Speedup $\text{Speedup}(p)$ Table

| Threads ($p$) | No Prune (`NP`) | Pruned (`P`) | Single Task + Pruned (`S`) | Single Task + No Prune (`SNP`) |
|:---:|:---:|:---:|:---:|:---:|
| **1** | $0.53\times$ | $1.01\times$ | $1.02\times$ | $0.70\times$ |
| **2** | $0.23\times$ | $1.95\times$ | $1.82\times$ | $0.40\times$ |
| **3** | $0.19\times$ | $1.94\times$ | $2.13\times$ | $0.31\times$ |
| **4** | $0.13\times$ | $2.37\times$ | $2.37\times$ | $0.24\times$ |
| **5** | $0.12\times$ | $2.50\times$ | $2.66\times$ | $0.22\times$ |
| **6** | $0.11\times$ | $3.48\times$ | $2.87\times$ | $0.19\times$ |
| **7** | $0.11\times$ | $3.48\times$ | $3.08\times$ | $0.18\times$ |
| **8** | $0.10\times$ | **$3.55\times$** | **$3.33\times$** | $0.16\times$ |

---

## 3. Analysis

### Task Creation & Work Distribution
* **Unpruned versions (`NP`, `SNP`):** `Merge_sortNP` creates approximately $2N - 1 \approx 200,000,000$ dynamic OpenMP tasks for $N = 100,000,000$. `Merge_sortSNP` halves this to $\sim 100,000,000$ tasks. Leaf-level tasks perform almost zero work (sorting 1–2 elements), meaning task creation, queue scheduling, and synchronization overhead drastically outweigh actual sorting work.
* **Pruned versions (`P`, `S`):** Introducing a cutoff threshold restricts task spawning to the upper levels of the recursion tree. This limits total task generation to a bounded set of large tasks while assigning substantial sequential work ($O(K \log K)$) to each task.

### Multi-Thread Behavior Across Versions
* **Pruned (`P` & `S`):** Adding threads yielded significant speedup, reducing runtime from $\sim 14.37\text{s}$ down to $4.10\text{s}$ (`P`) and $4.38\text{s}$ (`S`) at 8 threads.
* **Unpruned (`NP` & `SNP`):** Adding threads caused severe performance degradation rather than speedup. Runtime for `NP` worsened from $27.27\text{s}$ at 1 thread to $148.78\text{s}$ at 8 threads. Adding threads increases lock contention on OpenMP task queues, cache invalidation, and memory bandwidth saturation.

### Task Size & Pruning Impact
Changing the task cutoff threshold dictates the balance between parallel overhead and work granularity. Without pruning, task management overhead dominates. Setting a threshold ensures every thread executes meaningful sequential sorting before returning. If the threshold is set too large, thread utilization drops because too few tasks are available to keep all 8 cores busy.

### Scaling Bottlenecks & Sequential Segments
Speedup flattens out around 6–8 threads ($\sim 3.55\times$ speedup at 8 threads). The primary bottleneck is the **merge step**. While sub-array sorting is parallelized via recursive tasks, merging two sorted halves at higher recursion levels remains a strictly sequential $O(N)$ operation executed by a single thread after a `#pragma omp taskwait`. At the root level, one thread sequentially merges $100,000,000$ elements.

### Interaction Between Design Choices
The two independent variables (Pruning vs. Single-Task Spawning) **do not matter equally in both combinations**:
* **In the Unpruned regime:** Single-task spawning makes a massive difference, reducing 8-thread runtime from $148.78\text{s}$ (`NP`) to $89.40\text{s}$ (`SNP`) by eliminating $100,000,000$ task allocations.
* **In the Pruned regime:** Single-task spawning makes almost no difference. `Merge_sortP` ($4.10\text{s}$) and `Merge_sortS` ($4.38\text{s}$) perform similarly because task creation overhead is already negligible when task counts are small.

### Single-Thread Parallel vs. Sequential Baseline
* **Pruned (`P`: 14.37s, `S`: 14.35s):** Slightly faster or virtually identical to sequential ($T_{\text{baseline}} = 14.58\text{s}$). OpenMP task overhead is negligible when task counts are low.
* **Unpruned (`NP`: 27.27s, `SNP`: 20.97s):** Significantly slower than sequential due to the overhead of creating and scheduling millions of microscopic tasks on a single thread.

---

## 4. Final Recommendation & Future Work
* **Selected Version:** I would keep **`Merge_sortP`** (Pruned Two-Task). It achieved the highest overall speedup (**$3.55\times$** at 8 threads) and lowest wall-clock latency ($4.10\text{s}$).
* **Next Steps:** Given more time, I would measure performance across varying cutoff thresholds ($10^2$ to $10^6$) to find the exact optimal task granularity, and implement a **parallel merge phase** (using binary search partitioning) to eliminate the top-level sequential merge bottleneck.
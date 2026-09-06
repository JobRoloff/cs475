# CS 475: Parallel Programming

## Curriculum

### Lab 1: Where are the load imbalances in the Mandelbrot program?
### Assignment 1: Finding the load imbalance iteravely

## Performance Equations

### Work & Load Imbalance
- **Row Density (Work per row):**
  $$D[i] = \sum_{j=0}^{\text{width}-1} M(cr_j, ci_i, \text{max\_iter})$$
  *(where $M$ is the escape iteration count returned by `mandel_point` for pixel $(j, i)$)*

- **Thread Workload (Contiguous block of rows):**
  $$\text{Work}_k = \sum_{i \in \text{Rows}_k} D[i]$$

- **Average Workload per Thread:**
  $$\text{Work}_{\text{avg}} = \frac{\sum_{i=0}^{N-1} D[i]}{p}$$

- **Imbalance Ratio:**
  $$\text{Imbalance Ratio}(p) = \frac{\text{Work}_{\text{max}}}{\text{Work}_{\text{avg}}} = \frac{\max_{k}(\text{Work}_k)}{\text{Work}_{\text{avg}}}$$

---

### Parallel Performance Metrics
- **Baseline Time ($T_{\text{baseline}}$):**
  $$T_{\text{baseline}} = \min(T_{\text{seq}}, T(1))$$
  *(The execution time of the sequential run or single-threaded run)*

- **Speedup:**
  $$\text{Speedup}(p) = \frac{T_{\text{baseline}}}{T(p)}$$
  *(where $T(p)$ is the total wall-clock time using $p$ threads)*

- **Efficiency:**
  $$E(p) = \frac{\text{Speedup}(p)}{p} = \frac{T_{\text{baseline}}}{p \times T(p)}$$

- **Predicted Speedup Bounded by Imbalance:**
  $$\text{Speedup}_{\text{predicted}}(p) \approx \frac{p}{\text{Imbalance Ratio}(p)}$$

---

### Scheduling Parameters
- **Chunk Count:**
  $$\text{Total Chunks} = \left\lceil \frac{N}{\text{chunk\_size}} \right\rceil$$
  *(where $N = 2400$ total rows)*
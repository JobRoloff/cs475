import subprocess
from pathlib import Path
import pandas as pd
from pydantic import BaseModel, Field, model_validator
from .performance.analyzer import PerformanceAnalyzer, PerformanceMetric


class IterationConfig(BaseModel):
    """Validated configuration for an IterationRunner execution."""

    title: str = Field(
        ..., min_length=1, description="Title of the iteration benchmark"
    )
    description: str = Field(
        ..., description="Detailed description of what is being tested"
    )
    clause: str = Field(..., description="OpenMP clause being benchmarked")
    output_csv: Path = Field(
        default=Path("tmp_run.csv"), description="Path for intermediate results CSV"
    )
    p_min: int = Field(default=1, ge=1, description="Minimum thread count")
    p_max: int = Field(default=36, ge=1, description="Maximum thread count")
    runs: int = Field(
        default=1, ge=1, description="Number of runs per thread count"
    )

    # Cross-field validation rule
    @model_validator(mode="after")
    def validate_thread_bounds(self) -> "IterationConfig":
        if self.p_max < self.p_min:
            raise ValueError(
                f"p_max ({self.p_max}) cannot be smaller than p_min ({self.p_min})"
            )
        return self

    class Config:
        # Prevent modification after initialization (immutable config)
        frozen = True


class IterationRunner:
    """
    a reusable class for running a set of preformance tests on the mandelbrot.c program
    """

    def __init__(self, config: IterationConfig):
        self.config = config
        self.preformance_analyzer = PerformanceAnalyzer()

    def run_iteration(self) -> PerformanceMetric:
        """
        Thread count impacts program execution preformance
        
        Across all thread counts (pmin - pmax), this method measures the preformance of mandelbrot.c
        """
        # create preformance data
        data = self._execute_sweep()
        # compute the metrics from the sweep execution
        print("run_iteration: sweep ran")
        metrics: PerformanceMetric = (
            self.preformance_analyzer.compute_metrics(data, self.config.p_max)
        )
        return metrics

    def _execute_sweep(self) -> pd.DataFrame:
        """Runs sweep.sh to execute the parallel binary over thread counts."""
        print(
            f"[{self.config.title}] Running sweep from p={self.config.p_min} to p={self.config.p_max}..."
        )

        cmd = [
            "./bin/sweep.sh",
            "-pmin",
            str(self.config.p_min),
            "-pmax",
            str(self.config.p_max),
            "-r",
            str(self.config.runs),
            "-o",
            str(self.config.output_csv),
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            print("Sweep Execution Error:\n", result.stderr)
            raise RuntimeError("Failed to execute sweep script.")

        # Parse generated CSV into a Pandas DataFrame
        df = pd.read_csv(self.config.output_csv)
        return df

    def run_sweep(self) -> pd.DataFrame:
        return self._execute_sweep()

    def generate_report_block(self, metrics: PerformanceMetric) -> None:
        markdown_content = f"""
# Iteration: {self.config.title}
**Description:** {self.config.description}
**OpenMP Clause:** `{self.config.clause}`

### 1. Question
<!-- Insert Question here -->

### 2. Hypothesis
<!-- Insert falsifiable hypothesis here -->

### 3. Experiment Design
- Executed binary with `{self.config.clause}`
- Thread counts: p={self.config.p_min} to {self.config.p_max}
- Runs per thread count: {self.config.runs}

### 4. Analysis
{metrics}

*Refutation / Support Analysis:* 
<!-- Compare measured speedup against hypothesis -->

### 5. Conclusions
<!-- Next steps or final results -->
"""
        with open(self.config.output_csv, "a") as f:
            f.write(markdown_content)
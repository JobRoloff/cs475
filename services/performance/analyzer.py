from typing import Optional
import pandas as pd
from pydantic import BaseModel
from .types import PerformanceMetric

class PerformanceAnalyzer(BaseModel):
    baseline_time: Optional[float] = None
    baseline_speedup: Optional[float] = None
    imbalance_ratio: Optional[float] = None

    def compute_metrics(
        self,
        data: pd.DataFrame,
        threads: int,
    ) -> PerformanceMetric:
        """For a single thread run (p), calculates empirical performance metrics."""

        # 1. Compute baseline timing if not set
        if self.baseline_time is None or self.baseline_speedup is None:
            # Fix: Use column slice 1:6 (columns 1 to 5) instead of [1-5]
            observed_times = data.iloc[0, 1:6].astype(float)
            self.baseline_time = float(observed_times.min())

            sum_times = float(observed_times.sum())
            self.baseline_speedup = (
                self.baseline_time / sum_times if sum_times > 0 else 0.0
            )

        # 2. Extract timing for current thread count (0-indexed thread lookup)
        observed_times_p = data.iloc[threads - 1, 1:6].astype(float)
        total_time_sec_p = float(observed_times_p.sum())

        # 3. Calculate metrics
        speedup_p = self._calculate_speedup(total_time_sec_p)
        efficiency_p = self._calculate_efficiency(speedup_p, threads)

        predicted_speedup = None
        if self.imbalance_ratio and self.imbalance_ratio > 0:
            predicted_speedup = threads / self.imbalance_ratio

        return PerformanceMetric(
            threads=threads,
            time_sec=total_time_sec_p,
            speedup=speedup_p,
            efficiency=efficiency_p,
            predicted_speedup=predicted_speedup,
        )

    def _calculate_speedup(self, time_sec: float) -> float:
        # Type Guard: Ensure self.baseline_time is not None for static analysis
        if self.baseline_time is None or time_sec <= 0:
            return 0.0
        return self.baseline_time / time_sec

    def _calculate_efficiency(self, speedup: float, threads: int) -> float:
        if threads <= 0:
            return 0.0
        return speedup / threads
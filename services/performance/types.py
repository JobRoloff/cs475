from typing import Optional
import pandas as pd
from pydantic import ConfigDict, BaseModel, Field
class DataPayload(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)
    name: str
    data: pd.DataFrame


class PerformanceMetric(BaseModel):
    threads: int = Field(..., description="Number of threads (p)")
    time_sec: float = Field(..., description="Wall-clock time T(p)")
    speedup: float = Field(
        ..., description="Measured Speedup = T_baseline / T(p)"
    )
    efficiency: float = Field(
        ..., description="Measured Efficiency = Speedup / p"
    )
    predicted_speedup: Optional[float] = Field(
        None, description="Theoretical upper bound = p / ImbalanceRatio"
    )


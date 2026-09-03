from pydantic import BaseModel
class PreformanceAnalyzer(BaseModel):
    """
    we need to generate speedup and efficiency gains across experiments
    """
    # this is constant and used across parts of the file gen process
    __density_profile = "density_profile.csv"



    def calculate_analysis(self) -> None:
        """
        measures performance in a parallel program
        """
        static_speedup = self._calculate_static_speedup()
        static_efficiency = self._calculate_static_efficiency()

        dynamic_speedup = self._calculate_dynamic_speedup()
        dynamic_efficiency = self._calculate_dynamic_efficiency()

    def _calculate_static_speedup(self) -> float:
        """
        This Method,

        Assumes:

        Uses:
            - for time_baseline, we use max observed time
            - for time_p, we use average time across trials

        Outputs:
            - A float representing 
        """
        thread_count_p = 24
        time_baseline = 0
        time_p =  0
        return time_baseline / time_p

    def _calculate_static_efficiency(self) -> float:
        return 0.0

    def _calculate_dynamic_speedup(self) -> float:
        return 0.0

    def _calculate_dynamic_efficiency(self) -> float:
        return 0.0
from pydantic import BaseModel
from services.file_generator import FileGenerator
from services.preformance_report import PerformanceAnalyzer
from services.performance.analyzer import PerformanceAnalyzer

__file_generator = FileGenerator()
__performance_analyzer = PerformanceAnalyzer()

class PrrformanceReporter(BaseModel):
    input_file_name = str
    output_file_name = str

    def report(self) -> None:
        __data = __performance_analyzer.compute_metrics()
        __file_generator.export_data(__data, "some value")
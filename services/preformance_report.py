from pydantic import BaseModel
from services.file_generator import FileGenerator
from services.preformance_report import PreformanceAnalyzer
from services.preformance_analyzer import PreformanceAnalyzer

__file_generator = FileGenerator()
__preformance_analyzer = PreformanceAnalyzer()

class PerformanceReporter(BaseModel):
    input_file_name = str
    output_file_name = str

    def report(self) -> None:
        __data = __preformance_analyzer.calculate_analysis()
        __file_generator.export_data(__data, "some value")
import sys
from pathlib import Path

# Ensures the directory containing main.py is in the search path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

import argparse
from services.iteration_runner import IterationConfig, IterationRunner

if __name__ == "__main__":
    THREAD_COUNT = 24
    NUM_RUNS_PER_TRIAL = 5
    MIN_THREAD_COUNT = 1

    # 1. Parse Command Line Arguments
    parser = argparse.ArgumentParser(description="CS475 Parallel Programming PA 1")
    parser.add_argument(
        "-i",
        "--inputFileName",
        type=str,
        default="default_range.csv",
        help="Input data file path",
    )
    parser.add_argument(
        "-o",
        "--outputFileName",
        type=str,
        default="report.md",
        help="Output report markdown path",
    )

    args = parser.parse_args()

    input_file_name = args.inputFileName
    output_file_name = args.outputFileName

    # 2. Instantiate the Pydantic IterationConfig
    # Pydantic will validate that title/description are non-empty and p_min <= p_max
    iteration_config = IterationConfig(
        title="Static vs Dynamic Work Allocation",
        description="Comparing OpenMP static and dynamic scheduling clauses across varying thread counts.",
        clause="schedule(static)",
        output_csv=output_file_name,
        p_min=MIN_THREAD_COUNT,
        p_max=THREAD_COUNT,
        runs=NUM_RUNS_PER_TRIAL,
    )

    # 3. Pass the config instance directly to the IterationRunner
    iteration_runner = IterationRunner(config=iteration_config)

    iteration_metrics = iteration_runner.run_iteration()

    print("main: calling generate report block")
    iteration_runner.generate_report_block(iteration_metrics)

    # 4. Generate the performance report
    # performance_reporter = PerformanceReporter(
    #     output_file_name=output_file_name
    # )
    # performance_reporter.report()
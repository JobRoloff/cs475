import argparse
import numpy as np
import pandas as pd
from services.preformance_report import PerformanceReporter

if __name__ == "__main__":
    """

    This Program,
    
    Assumes:
    - default range
        this keeps the density profile constant
    - constant num threads
        - this keeps work imbalance between threads (p) constant

    Computes:
    - Static Speedup
    - Static Efficiency
    - Dynamic Speedup
    - Dynamic Efficiency
    
    Outputs:
    - a markdown file reporting changes in program performance based on computed data

    """
    parser = argparse.ArgumentParser(description="CS475 Parallel Programming PA 1")

    parser.add_argument("-i", "--inputFileName", type=str, default="", help="The name of the user")
    parser.add_argument("-o", "--outputFileName", type=str, default="", help="The name of the user")

    args = parser.parse_args()

    input_file_name = f"{args.inputFileName}"
    output_file_name = f"{args.outputFileName}"

    reporter_dependencies = {
        "input_file_name" : input_file_name,
        "output_file_name" : output_file_name,
    }

    # although you can directly add the parameters, into the constructor, you could unpack a dictionary. Doing so sets us up for JSON payload / API usage
    performance_reporter = PerformanceReporter(**reporter_dependencies)

    performance_reporter.report()


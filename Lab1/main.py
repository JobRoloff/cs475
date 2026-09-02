"""
implementation limitations: could leverage python for in-memory data frame computations rather than create maps
"""
import numpy as np
import pandas as pd
import csv

from typing import Dict, List, Any

def sum_density_values(value_map: pd.DataFrame):
    return value_map['density'].sum()

def split_dataframe(df: pd.DataFrame, num_chunks: int) -> List[pd.DataFrame]:
    chunk_size, remainder = divmod(len(df), num_chunks)
    chunks = []
    start = 0
    for i in range(num_chunks):
        end = start + chunk_size + (1 if i < remainder else 0)
        chunks.append(df.iloc[start:end])
        start = end
    return chunks
def average_work_per_chunk():
    return
if __name__ == "__main__":
    df = pd.read_csv("density_profile.csv")

    thread_counts = [3, 6, 8, 24]
    
    # Store List[pd.DataFrame] for each thread count key

    chunk_map_by_thread: Dict[int, List[pd.DataFrame]] = {}

    # Fill the data structure
    for count in thread_counts:
        chunk_map_by_thread[count] = split_dataframe(df, count)

    # Calculate work imbalance per thread count

    # storage to write to our output file
    report_rows = []

    # Imbalance Step 1: sum of values per thread. note : numerator is greatest df sum
    for p, chunks in chunk_map_by_thread.items():
        # store sum of each chunk into an array
        chunk_sums = [sum_density_values(chunk) for chunk in chunks]
        # get max and average across the thread
        max_work = max(chunk_sums)
        avg_work = sum(chunk_sums) / p
        # compute imbalance ratio. be sure to avoid divide by 0 error
        imbalance_ratio = max_work / avg_work if avg_work != 0 else 0.0
        # format maths done for our appending to csv
        work_shown_str = (
            f"max({[round(s, 2) for s in chunk_sums]}) / ({round(sum(chunk_sums), 2)} / {p}) "
            f"= {max_work:.2f} / {avg_work:.2f}"
        )

        report_rows.append(
            {"p": p, "ratio": round(imbalance_ratio, 4), "work_shown": work_shown_str}
        )

    #  write to output file

    output_filename = "work_imbalance.csv"
    with open(output_filename, mode="w", newline="") as csv_file:
        fieldnames = ["p", "ratio", "work_shown"]
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)

        writer.writeheader()
        writer.writerows(report_rows)
    print(f"Report writen to {output_filename}")

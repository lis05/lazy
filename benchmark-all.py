#!/usr/bin/env python3
"""Run all parameter generators and execute benchmarks."""

from benchmark import benchmark_row
import sys
import os
import subprocess
import csv
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))

PARAMS_DIR = "benchmark-params"
RESULTS_DIR = "benchmark-results"
MASTER_RESULT = f"{RESULTS_DIR}/results.csv"


def run_generator(py_file):
    """Run a parameter generator script."""
    try:
        result = subprocess.run(
            ["python3", py_file.name],
            cwd=PARAMS_DIR,
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"  stderr: {result.stderr}", file=sys.stderr)
        return result.returncode == 0
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return False


def main():
    # Check command-line arguments
    if len(sys.argv) < 2:
        print("Usage: python3 benchmark-all.py <input_file>", file=sys.stderr)
        print("  input_file: Path to the file to benchmark", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]

    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found.", file=sys.stderr)
        sys.exit(1)

    # Check prerequisites
    if not os.path.isdir(PARAMS_DIR):
        print(
            f"Error: Parameters directory '{PARAMS_DIR}' does not exist.", file=sys.stderr)
        sys.exit(1)

    os.makedirs(RESULTS_DIR, exist_ok=True)

    # Step 1: Clean old master results file
    if os.path.exists(MASTER_RESULT):
        os.remove(MASTER_RESULT)

    # Step 4: Clean up generated CSV files
    for csv_file in Path(PARAMS_DIR).glob("*.csv"):
        try:
            os.remove(csv_file)
        except Exception:
            pass

    # Step 2: Run all Python scripts to generate CSV parameter files
    py_files = sorted(Path(PARAMS_DIR).glob("*.py"))
    for py_file in py_files:
        print(f"Executing generator: {py_file.name}...", file=sys.stderr)
        if not run_generator(py_file):
            print(
                f"Warning: Generator {py_file.name} failed.", file=sys.stderr)

    # Step 3: Run benchmarks and write results immediately
    csv_files = sorted(Path(PARAMS_DIR).glob("*.csv"))
    csv_writer = None
    fieldnames = None

    for params_file in csv_files:
        filename = params_file.name

        if not params_file.exists():
            print(
                f"Warning: Parameter file {filename} not found, skipping.", file=sys.stderr)
            continue

        print(f"Running benchmark for {filename}...", file=sys.stderr)

        # Read parameters and benchmark each row
        try:
            with open(params_file, 'r') as f:
                reader = csv.DictReader(f)
                rows = list(reader)
                total_rows = len(rows)

                for row_idx, param_row in enumerate(rows, 1):
                    print(
                        f"  Progress: {row_idx}/{total_rows} ({100*row_idx//total_rows}%)", file=sys.stderr)
                    # Inject the input file from command-line argument
                    param_row['file'] = input_file
                    result = benchmark_row(param_row)

                    if result:
                        # Initialize CSV writer on first result
                        if csv_writer is None:
                            fieldnames = list(result.keys())
                            output_file = open(MASTER_RESULT, 'a', newline='')
                            csv_writer = csv.DictWriter(
                                output_file, fieldnames=fieldnames)
                            csv_writer.writeheader()

                        # Write result immediately
                        csv_writer.writerow(result)
                        output_file.flush()
        except FileNotFoundError:
            print(
                f"Error: Parameter file {filename} not found.", file=sys.stderr)
            continue
        except Exception as e:
            print(f"Error processing {filename}: {e}", file=sys.stderr)
            continue

        print(f"Finished {filename} -> {MASTER_RESULT}", file=sys.stderr)

    # Close output file
    if csv_writer:
        output_file.close()

    print(
        f"All benchmarks completed. Output saved to {MASTER_RESULT}", file=sys.stderr)


if __name__ == "__main__":
    main()

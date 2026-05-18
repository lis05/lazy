#!/usr/bin/env python3
"""Run all parameter generators and execute benchmarks."""

import sys
import os
import subprocess
from pathlib import Path

PARAMS_DIR = "benchmark-params"
RESULTS_DIR = "benchmark-results"
BENCHMARK_SCRIPT = "benchmark.py"
MASTER_RESULT = f"{RESULTS_DIR}/results.csv"


def run_command(cmd):
    """Run command and return return code."""
    try:
        result = subprocess.run(cmd, shell=True)
        return result.returncode
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


def main():
    # Check prerequisites
    if not os.path.isdir(PARAMS_DIR):
        print(
            f"Error: Parameters directory '{PARAMS_DIR}' does not exist.", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(BENCHMARK_SCRIPT):
        print(
            f"Error: Benchmark script '{BENCHMARK_SCRIPT}' not found.", file=sys.stderr)
        sys.exit(1)

    os.makedirs(RESULTS_DIR, exist_ok=True)

    # Step 1: Clean old master results file
    if os.path.exists(MASTER_RESULT):
        os.remove(MASTER_RESULT)

    # Step 2: Run all Python scripts to generate CSV parameter files
    py_files = sorted(Path(PARAMS_DIR).glob("*.py"))
    for py_file in py_files:
        print(f"Executing generator: {py_file.name}...", file=sys.stderr)
        cmd = f"cd {PARAMS_DIR} && python3 {py_file.name}"
        if run_command(cmd) != 0:
            print(
                f"Warning: Generator {py_file.name} failed.", file=sys.stderr)

    # Step 3: Run benchmarks and append to the single master file
    csv_files = sorted(Path(PARAMS_DIR).glob("*.csv"))
    is_first_file = True

    for params_file in csv_files:
        filename = params_file.name
        print(f"Running benchmark for {filename}...", file=sys.stderr)

        if is_first_file:
            # Keep header for the first file
            cmd = f"python3 {BENCHMARK_SCRIPT} '{params_file}' >> {MASTER_RESULT}"
            run_command(cmd)
            is_first_file = False
        else:
            # Strip header for subsequent files
            cmd = f"python3 {BENCHMARK_SCRIPT} '{params_file}' | tail -n +2 >> {MASTER_RESULT}"
            run_command(cmd)

        print(f"Finished {filename} -> {MASTER_RESULT}", file=sys.stderr)

    # Step 4: Clean up generated CSV files
    for csv_file in csv_files:
        try:
            os.remove(csv_file)
        except Exception:
            pass

    print(
        f"All benchmarks completed. Output saved to {MASTER_RESULT}", file=sys.stderr)


if __name__ == "__main__":
    main()

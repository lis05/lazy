#!/usr/bin/env python3
"""Benchmark script for LZ77 compression with multiple runs and verification."""

import sys
import os
import subprocess
import tempfile
import csv
from pathlib import Path

BINARY = "build/lz77"
TEMP_ENC = "/tmp/encoded"
TEMP_DEC = "/tmp/decoded"
OUTPUT_DIR = "benchmark-results"
OUTPUT_FILE = f"{OUTPUT_DIR}/results.csv"
NUM_RUNS = 3


def run_command(cmd):
    """Run command and return output."""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True)
        return result.stdout + result.stderr
    except Exception as e:
        return f"Error: {e}"


def extract_time(output):
    """Extract TIME value from command output."""
    for line in output.split('\n'):
        if 'TIME:' in line:
            parts = line.split()
            if len(parts) >= 2:
                try:
                    return float(parts[1])
                except ValueError:
                    pass
    return 0.0


def benchmark_row(params):
    """Benchmark a single parameter set."""
    input_file = params['file']
    format_type = params['format']
    block_size = params['block_size']
    window_size = params['window_size']
    future_limit = params['future_limit']
    max_matches = params['max_matches']

    if not os.path.exists(input_file):
        print(
            f"Error: Input file '{input_file}' not found. Skipping row.", file=sys.stderr)
        return None

    orig_size = os.path.getsize(input_file)

    # Run encoding 3 times
    enc_times = []
    for i in range(NUM_RUNS):
        cmd = (f"{BINARY} -e -i '{input_file}' -o '{TEMP_ENC}' -f '{format_type}' -m "
               f"--block-size {block_size} "
               f"--window-size {window_size} "
               f"--future-limit {future_limit} "
               f"--max-matches {max_matches}")
        output = run_command(cmd)
        time_val = extract_time(output)
        enc_times.append(time_val)

    # Run decoding 3 times
    dec_times = []
    for i in range(NUM_RUNS):
        cmd = (f"{BINARY} -d -i '{TEMP_ENC}' -o '{TEMP_DEC}' -m "
               f"--block-size {block_size} "
               f"--window-size {window_size} "
               f"--future-limit {future_limit} "
               f"--max-matches {max_matches}")
        output = run_command(cmd)
        time_val = extract_time(output)
        dec_times.append(time_val)

    # Compute average times
    enc_time_avg = sum(enc_times) / len(enc_times) if enc_times else 0
    dec_time_avg = sum(dec_times) / len(dec_times) if dec_times else 0
    enc_size = os.path.getsize(TEMP_ENC) if os.path.exists(TEMP_ENC) else 0

    # Verify decoded file matches original
    if not os.path.exists(TEMP_DEC) or not files_equal(input_file, TEMP_DEC):
        print(f"\nError: Verification failed!", file=sys.stderr)
        print(f"Failed parameters:", file=sys.stderr)
        print(f"  file:            {input_file}", file=sys.stderr)
        print(f"  format:          {format_type}", file=sys.stderr)
        print(f"  block_size:      {block_size}", file=sys.stderr)
        print(f"  window_size:     {window_size}", file=sys.stderr)
        print(f"  future_limit:    {future_limit}", file=sys.stderr)
        print(f"  max_matches:     {max_matches}", file=sys.stderr)
        return None

    # Calculate speeds and ratio
    enc_mbps = (orig_size / 1048576) / enc_time_avg if enc_time_avg > 0 else 0
    dec_mbps = (orig_size / 1048576) / dec_time_avg if dec_time_avg > 0 else 0
    ratio = (enc_size / orig_size * 100) if orig_size > 0 else 0

    return {
        'file': input_file,
        'format': format_type,
        'block_size': block_size,
        'window_size': window_size,
        'future_limit': future_limit,
        'max_matches': max_matches,
        'enc_speed_mbps': f"{enc_mbps:.2f}",
        'dec_speed_mbps': f"{dec_mbps:.2f}",
        'enc_size': enc_size,
        'ratio': f"{ratio:.2f}%"
    }


def files_equal(file1, file2):
    """Compare two files byte by byte."""
    try:
        with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
            while True:
                chunk1 = f1.read(8192)
                chunk2 = f2.read(8192)
                if chunk1 != chunk2:
                    return False
                if not chunk1:
                    return True
    except Exception:
        return False


def main():
    if len(sys.argv) != 2:
        print("Usage: benchmark.py <params_file>", file=sys.stderr)
        sys.exit(1)

    params_file = sys.argv[1]

    if not os.path.exists(params_file):
        print(
            f"Error: Parameters file '{params_file}' not found.", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(BINARY):
        print(f"Error: Binary '{BINARY}' not found.", file=sys.stderr)
        sys.exit(1)

    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Read parameters
    rows_to_process = []
    with open(params_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows_to_process.append(row)

    total_rows = len(rows_to_process)
    print(f"Writing results to {OUTPUT_FILE}", file=sys.stderr)

    # Process rows and write results
    results = []
    for idx, param_row in enumerate(rows_to_process, 1):
        print(
            f"\rProgress: {idx}/{total_rows} ({100*idx//total_rows}%)", end='', file=sys.stderr)
        result = benchmark_row(param_row)
        if result:
            results.append(result)

    print(f"\nFinished processing all benchmarks.", file=sys.stderr)

    # Write output CSV
    if results:
        fieldnames = list(results[0].keys())
        with open(OUTPUT_FILE, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)

    print(f"Results saved to: {OUTPUT_FILE}", file=sys.stderr)

    # Cleanup
    for temp_file in [TEMP_ENC, TEMP_DEC]:
        if os.path.exists(temp_file):
            os.remove(temp_file)


if __name__ == "__main__":
    main()

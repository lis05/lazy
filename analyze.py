#!/usr/bin/env python3
"""Analyze benchmark results and display tables."""

import sys
import os
import csv
import math
from typing import List, Dict

RESULTS_FILE = "benchmark-results/results.csv"


def read_results() -> List[Dict]:
    """Read results CSV file."""
    if not os.path.exists(RESULTS_FILE):
        print(
            f"Error: Result file '{RESULTS_FILE}' not found.", file=sys.stderr)
        sys.exit(1)

    results = []
    with open(RESULTS_FILE, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            results.append(row)
    return results


def print_header():
    """Print analysis header."""
    print("=========================================================")
    print("               BENCHMARK ANALYSIS RESULTS                ")
    print("=========================================================")


def print_table(rows: List[Dict]):
    """Print formatted table."""
    # Format: left-aligned columns with | separators
    print(f"{'Format':<6} | {'Block Size':<10} | {'Window Size':<11} | {'Future Limit':<12} | {'Max Matches':<11} | {'Enc MB/s':<8} | {'Dec MB/s':<8} | {'Ratio':<8}")
    print("-" * 95)

    for row in rows:
        format_val = row.get('format', '')
        block_size = row.get('block_size', '')
        window_size = row.get('window_size', '')
        future_limit = row.get('future_limit', '')
        max_matches = row.get('max_matches', '')
        enc_mbps = row.get('enc_speed_mbps', '')
        dec_mbps = row.get('dec_speed_mbps', '')
        ratio = row.get('ratio', '')

        print(f"{format_val:<6} | {block_size:<10} | {window_size:<11} | {future_limit:<12} | {max_matches:<11} | {enc_mbps:<8} | {dec_mbps:<8} | {ratio:<8}")


def best_by_ratio(results: List[Dict]):
    """Find 5 best compressors by ratio."""
    print("\n[ 5 BEST COMPRESSORS BY RATIO ]")

    def get_ratio(row):
        ratio_str = row.get('ratio', '0%').strip('%')
        try:
            return float(ratio_str)
        except ValueError:
            return float('inf')

    sorted_results = sorted(results, key=get_ratio)[:5]
    print_table(sorted_results)


def best_by_enc_speed(results: List[Dict]):
    """Find 5 best compressors by encoding speed."""
    print("\n[ 5 BEST COMPRESSORS BY ENCODING SPEED ]")

    def get_enc_speed(row):
        try:
            return float(row.get('enc_speed_mbps', '0'))
        except ValueError:
            return 0

    sorted_results = sorted(results, key=get_enc_speed, reverse=True)[:5]
    print_table(sorted_results)


def best_by_dec_speed(results: List[Dict]):
    """Find 5 best compressors by decoding speed."""
    print("\n[ 5 BEST COMPRESSORS BY DECODING SPEED ]")

    def get_dec_speed(row):
        try:
            return float(row.get('dec_speed_mbps', '0'))
        except ValueError:
            return 0

    sorted_results = sorted(results, key=get_dec_speed, reverse=True)[:5]
    print_table(sorted_results)


def best_overall_balanced(results: List[Dict]):
    """Find 5 best compressors by balanced score."""
    print("\n[ 5 BEST COMPRESSORS OVERALL (BALANCED SCORE) ]")

    def calculate_score(row):
        ratio_str = row.get('ratio', '0%').strip('%')
        enc_str = row.get('enc_speed_mbps', '0')
        dec_str = row.get('dec_speed_mbps', '0')

        try:
            ratio = float(ratio_str)
            enc = float(enc_str)
            dec = float(dec_str)
        except ValueError:
            return float('inf')

        if enc <= 0:
            enc = 0.1
        if dec <= 0:
            dec = 0.1

        return ratio / (math.log(enc) + math.log(dec))

    sorted_results = sorted(results, key=calculate_score)[:5]
    print_table(sorted_results)


def main():
    results = read_results()

    if not results:
        print("No results to analyze.", file=sys.stderr)
        sys.exit(1)

    print_header()
    best_by_ratio(results)
    best_by_enc_speed(results)
    best_by_dec_speed(results)
    best_overall_balanced(results)


if __name__ == "__main__":
    main()

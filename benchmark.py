#!/usr/bin/env python3
import csv
import filecmp
import os
import subprocess
import sys
import time

# Configuration
LZMPO_BIN = "./build/lzmpo"
INPUT_FILE = os.path.expanduser("datasets/enwik/enwik8")
ENCODED_FILE = "/tmp/encoded"
DECODED_FILE = "/tmp/decoded"
CSV_FILE = "lzmpo_benchmark_results.csv"

# Constant arguments for the compressor
CONST_ARGS = [
    "--max",
    "--mm",
    "200",
    "-p",
    "-k",
    "16",
]

# List of --pl values to test (14 items each)
PL_LISTS = [
    # 2 groups of 7. Steps: +1, +2.
    "6,7,8,9,10,11,12," + "14,16,18,20,22,24,26",
    # 2 groups of 7. Steps: +1, +3.
    "6,7,8,9,10,11,12," + "15,18,21,24,27,30,33",
    # 4 groups of 3, 1 group of 2. Steps: +1, +2, +3, +4, +5.
    "6,7,8," + "10,12,14," + "17,20,23," + "27,31,35," + "40,45",
    # 4 groups of 3, 1 group of 2. Steps grow 1.7x.
    "6,7,8," + "10,12,14," + "17,21,25," + "30,37,44," + "54,66",
    # 1 group of 2, 4 groups of 3. Steps grow 1.5x.
    "6,7," + "9,11,13," + "16,19,22," + "26,30,34," + "40,46,52",
    # 1 group of 2, 4 groups of 3. Steps grow 1.4x.
    "6,7," + "9,11,13," + "16,19,22," + "26,30,34," + "39,44,49",
    # 14 items. Constant step: +1.
    "6,7,8,9,10,11,12,13,14,15,16,17,18,19",
    # 14 items. Steps grow linearly: +1, +2, +3...
    "6,7,9,12,16,21,27,34,42,51,61,72,84,97",
    # 14 items. Steps grow exponentially: 1.40^n.
    "6,7,8,10,12,16,21,28,37,50,68,93,128,177",
    # 14 items. Steps grow exponentially: 1.35^n.
    "6,7,8,10,12,15,19,25,32,42,55,72,95,126",
    # 14 items. Steps grow exponentially: 1.30^n.
    "6,7,8,10,12,15,19,24,30,38,49,63,81,105",
    # 14 items. Steps grow exponentially: 1.25^n.
    "6,7,8,10,11,14,17,20,24,29,35,43,53,65",
    # 14 items. Steps grow exponentially: 1.20^n.
    "6,7,8,9,11,13,15,18,21,24,28,33,38,44",
    # 14 items. Steps grow exponentially: 1.15^n.
    "6,7,8,10,11,13,15,17,19,22,25,28,32,36",
    # 14 items. Steps grow exponentially: 1.10^n.
    "6,7,8,9,10,12,13,15,16,18,20,22,24,26",
]


def run_and_measure(cmd):
    """Executes the given command, tracking execution time and maximum virtual

    memory usage (RAM + Swap) via high-frequency polling of /proc/[pid]/status
    VmPeak. Streams stdout in real-time to allow progress printing (-p).
    """
    start_time = time.time()
    max_vmpeak_kb = 0
    returncode = 0

    try:
        proc = subprocess.Popen(cmd, stdout=sys.stdout, stderr=sys.stderr, text=True)

        pid = proc.pid
        status_file = f"/proc/{pid}/status"

        # Poll /proc/[pid]/status to catch VmPeak (virtual memory peak, RAM + Swap)
        while proc.poll() is None:
            try:
                with open(status_file, "r") as f:
                    for line in f:
                        if line.startswith("VmPeak:"):
                            # Format: "VmPeak:   2048452 kB"
                            parts = line.split()
                            if len(parts) >= 2:
                                val = int(parts[1])
                                if val > max_vmpeak_kb:
                                    max_vmpeak_kb = val
                            break
            except FileNotFoundError:
                # Process might have just terminated
                pass
            time.sleep(0.1)

        # Final check if VmPeak is still readable to ensure accuracy
        try:
            with open(status_file, "r") as f:
                for line in f:
                    if line.startswith("VmPeak:"):
                        parts = line.split()
                        if len(parts) >= 2:
                            val = int(parts[1])
                            if val > max_vmpeak_kb:
                                max_vmpeak_kb = val
                        break
        except FileNotFoundError:
            pass

        returncode = proc.returncode

    except Exception as e:
        print(f"Error tracking process: {e}")
        if "proc" in locals():
            proc.kill()
        returncode = -1

    end_time = time.time()
    duration = end_time - start_time
    max_vmpeak_mb = max_vmpeak_kb / 1024.0

    return duration, max_vmpeak_mb, returncode


def main():
    if not os.path.isfile(INPUT_FILE):
        print(f"Error: Input file {INPUT_FILE} not found.")
        sys.exit(1)

    original_size = os.path.getsize(INPUT_FILE)

    # Identify already completed tests
    tested_configs = set()
    csv_exists = os.path.isfile(CSV_FILE)
    if csv_exists:
        with open(CSV_FILE, "r", newline="") as f:
            reader = csv.reader(f)
            next(reader, None)  # Skip header
            for row in reader:
                if row:
                    tested_configs.add(row[0])

    configs_to_run = [pl for pl in PL_LISTS if pl not in tested_configs]
    total_tests = len(configs_to_run)

    if total_tests == 0:
        print("All configurations have already been tested. Exiting.")
        sys.exit(0)

    print(f"Total tests to run: {total_tests}")

    # Open CSV in append mode
    with open(CSV_FILE, "a", newline="") as f:
        writer = csv.writer(f)
        if not csv_exists:
            writer.writerow(
                [
                    "pl_config",
                    "encode_time_s",
                    "encode_mem_mb",
                    "decode_time_s",
                    "decode_mem_mb",
                    "encoded_size_bytes",
                    "compression_ratio_pct",
                ]
            )

        for index, pl in enumerate(configs_to_run, start=1):
            print(
                f"\n[{index}/{total_tests}] Running test for configuration: --pl {pl}"
            )

            # 1. Encode
            encode_cmd = (
                [LZMPO_BIN, "-e", "-i", INPUT_FILE, "-o", ENCODED_FILE]
                + CONST_ARGS
                + ["--pl", pl]
            )
            print("  Encoding...")
            enc_time, enc_mem, enc_ret = run_and_measure(encode_cmd)
            if enc_ret != 0:
                print(f"  Encoding failed (Code: {enc_ret}). Skipping configuration.")
                continue

            # 2. Decode
            decode_cmd = (
                [LZMPO_BIN, "-d", "-i", ENCODED_FILE, "-o", DECODED_FILE]
                + CONST_ARGS
                + ["--pl", pl]
            )
            print("  Decoding...")
            dec_time, dec_mem, dec_ret = run_and_measure(decode_cmd)
            if dec_ret != 0:
                print(f"  Decoding failed (Code: {dec_ret}). Skipping configuration.")
                continue

            # 3. Verify
            print("  Verifying decoded file...")
            if not filecmp.cmp(INPUT_FILE, DECODED_FILE, shallow=False):
                print(
                    "  Verification failed: The decoded file does not match the original. Skipping configuration."
                )
                continue

            # 4. Metrics
            encoded_size = os.path.getsize(ENCODED_FILE)
            ratio_pct = (
                (encoded_size / original_size) * 100.0 if original_size > 0 else 0.0
            )

            # 5. Write and Flush
            writer.writerow(
                [
                    pl,
                    f"{enc_time:.4f}",
                    f"{enc_mem:.2f}",
                    f"{dec_time:.4f}",
                    f"{dec_mem:.2f}",
                    encoded_size,
                    f"{ratio_pct:.4f}%",
                ]
            )
            f.flush()

            print(
                f"  Success -> Enc Time: {enc_time:.2f}s | Enc Mem (VmPeak): {enc_mem:.2f} MB | Dec Time: {dec_time:.2f}s | Dec Mem (VmPeak): {dec_mem:.2f} MB | Ratio: {ratio_pct:.4f}%"
            )

    # Clean up temporary files
    if os.path.exists(ENCODED_FILE):
        os.remove(ENCODED_FILE)
    if os.path.exists(DECODED_FILE):
        os.remove(DECODED_FILE)
    print("\nBenchmarking complete.")


if __name__ == "__main__":
    main()

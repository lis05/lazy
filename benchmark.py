#!/bin/env python3
import subprocess
import filecmp
import time
import sys
import os
import itertools
import csv

compressor = "./build/lazy"


def encode(src, dest, args):
    args_str = " ".join(map(str, args))
    try:
        subprocess.run(
            [compressor, "-e", "-i", src, "-o", dest, *args], check=True
        )
    except:
        print(f"Encode ({compressor} -e -i {src} -o {dest} {args_str}) failed.")
        sys.exit(1)


def decode(src, dest, args):
    args_str = " ".join(map(str, args))
    try:
        subprocess.run(
            [compressor, "-d", "-i", src, "-o", dest, *args], check=True
        )
    except:
        print(f"Decode ({compressor} -d -i {src} -o {dest} {args_str}) failed.")
        sys.exit(1)


def compare(src, dest):
    if not filecmp.cmp(src, dest, shallow=False):
        print(f"Files are not the same: {src} and {dest}")


def gen_normal_tests():
    levels = ("-l", (0,))
    formats = ("-f", ("ctx",))
    jobs = ("-j", (16,))
    block_size = ("--bs", [1 << x for x in range(15, 21, 1)])
    window_size = ("--ws", [1 << x for x in range(15, 21, 1)])
    future_limit = ("--fl", [1 << x for x in range(15, 21, 1)])
    max_matches = ("--mm", (10, 1000, 0))
    lazy_matching = ("--lm", (0, 1, 2, 3, 4))

    params = [
        levels,
        formats,
        jobs,
        block_size,
        window_size,
        future_limit,
        max_matches,
        lazy_matching,
    ]

    keys = [item[0] for item in params]
    value_lists = [item[1] for item in params]

    tests = []

    for combination in itertools.product(*value_lists):
        test = dict(zip(keys, combination))

        if test[block_size[0]] < test[window_size[0]]:
            continue
        if test[block_size[0]] < test[future_limit[0]]:
            continue

        tests += [test]

    return tests


def measure(fn, *args):
    before = time.time()
    fn(*args)
    return time.time() - before


def run_test(file, test):
    encoded = "/tmp/encoded_benchmark"
    decoded = "/tmp/decoded_benchmark"

    stringified = " ".join(
        f"{a} {b}" if not isinstance(b, bool) else (a if b else "")
        for a, b in test.items()
    )

    enc_time = measure(encode, file, encoded, stringified.split())
    dec_time = measure(decode, encoded, decoded, stringified.split())
    compare(file, decoded)

    orig = os.path.getsize(file)
    enc = os.path.getsize(encoded)
    ratio = round(100 * enc / orig, 1) if orig > 0 else 0

    return [orig, enc, ratio, enc_time, dec_time]


def get_past_row(past_results, config, filename):
    for row in past_results:
        if row.get("filename") != filename:
            continue
        match = True
        for k, v in config.items():
            if str(row.get(k)) != str(v):
                match = False
                break
        if match:
            return row
    return None


def run_all_tests(folder, csv_filename, start_index):
    if not os.path.isdir(folder):
        print(f"Error: Directory '{folder}' does not exist.")
        sys.exit(1)

    files = [
        os.path.join(folder, f)
        for f in os.listdir(folder)
        if os.path.isfile(os.path.join(folder, f))
    ]
    if not files:
        print(f"Error: No files found in '{folder}'.")
        sys.exit(1)

    tests = gen_normal_tests()
    if not tests:
        print("No configurations to test.")
        return

    expected_fieldnames = list(tests[0].keys()) + [
        "filename", "orig_size", "enc_size", "ratio", "enc_time", "dec_time"
    ]

    operations = []
    for test in tests:
        for f in files:
            operations.append({"type": "test", "config": test, "file": f})
        operations.append({"type": "avg", "config": test})

    csv_valid = False
    past_results = []
    if os.path.exists(csv_filename):
        try:
            with open(csv_filename, "r", newline="") as f:
                reader = csv.DictReader(f)
                if reader.fieldnames and set(expected_fieldnames).issubset(set(reader.fieldnames)):
                    past_results = list(reader)
                    csv_valid = True
        except Exception:
            pass

    file_mode = "a" if csv_valid else "w"

    total = len(tests) * (len(files) + 1)

    with open(csv_filename, file_mode, newline="") as f:
        writer = csv.DictWriter(f, fieldnames=expected_fieldnames)
        if file_mode == "w":
            writer.writeheader()
            f.flush()

        current_sums = {"orig": 0, "enc": 0, "enc_time": 0.0, "dec_time": 0.0, "count": 0}

        for i, op in enumerate(operations):
            if i < start_index:
                if op["type"] == "test":
                    row = get_past_row(past_results, op["config"], op["file"])
                    if row:
                        current_sums["orig"] += float(row["orig_size"])
                        current_sums["enc"] += float(row["enc_size"])
                        current_sums["enc_time"] += float(row["enc_time"])
                        current_sums["dec_time"] += float(row["dec_time"])
                        current_sums["count"] += 1
                elif op["type"] == "avg":
                    current_sums = {"orig": 0, "enc": 0, "enc_time": 0.0, "dec_time": 0.0, "count": 0}
                continue

            if op["type"] == "test":
                print(f"[{i}/{total}] Running file: {op['file']} | config: {op['config']}")
                orig, enc, ratio, enc_time, dec_time = run_test(op["file"], op["config"])

                current_sums["orig"] += orig
                current_sums["enc"] += enc
                current_sums["enc_time"] += enc_time
                current_sums["dec_time"] += dec_time
                current_sums["count"] += 1

                row = op["config"].copy()
                row["filename"] = op["file"]
                row["orig_size"] = orig
                row["enc_size"] = enc
                row["ratio"] = ratio
                row["enc_time"] = enc_time
                row["dec_time"] = dec_time

                writer.writerow(row)
                f.flush()
                os.fsync(f.fileno())

            elif op["type"] == "avg":
                print(f"[{i}/{total}] Computing AVERAGE | config: {op['config']}")
                n = current_sums["count"]
                
                avg_orig = current_sums["orig"] / n if n > 0 else 0
                avg_enc = current_sums["enc"] / n if n > 0 else 0
                avg_ratio = round(100 * current_sums["enc"] / current_sums["orig"], 1) if current_sums["orig"] > 0 else 0
                avg_enc_time = current_sums["enc_time"] / n if n > 0 else 0
                avg_dec_time = current_sums["dec_time"] / n if n > 0 else 0

                row = op["config"].copy()
                row["filename"] = "AVERAGE"
                row["orig_size"] = avg_orig
                row["enc_size"] = avg_enc
                row["ratio"] = avg_ratio
                row["enc_time"] = avg_enc_time
                row["dec_time"] = avg_dec_time

                writer.writerow(row)
                f.flush()
                os.fsync(f.fileno())

                current_sums = {"orig": 0, "enc": 0, "enc_time": 0.0, "dec_time": 0.0, "count": 0}


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        print(f"Usage: {sys.argv[0]} <input_folder> <output_csv> [start_index]")
        sys.exit(1)

    input_folder = sys.argv[1]
    output_csv = sys.argv[2]
    start_index = int(sys.argv[3]) if len(sys.argv) == 4 else 0

    run_all_tests(input_folder, output_csv, start_index)


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
        res = subprocess.run(
            [compressor, "-e", "-i", src, "-o", dest, *args], check=True
        )
    except:
        print(f"Encode ({compressor} -e -i {src} -o {dest} {args_str}) failed.")
        exit(1)


def decode(src, dest, args):
    args_str = " ".join(map(str, args))
    try:
        res = subprocess.run(
            [compressor, "-d", "-i", src, "-o", dest, *args], check=True
        )
    except:
        print(f"Decode ({compressor} -d -i {src} -o {dest} {args_str}) failed.")
        exit(1)


def compare(src, dest):
    if not filecmp.cmp(src, dest, shallow=False):
        print(f"Files are not the same: {src} and {dest}")


def gen_normal_tests():
    levels = ("-l", (0,))
    formats = ("-f", ("ctx",))
    jobs = ("-j", (1, 2, 4, 8, 16))
    block_size = ("--bs", [1 << x for x in range(14, 21, 1)])
    window_size = ("--ws", [1 << x for x in range(12, 21, 1)])
    future_limit = ("--fl", [1 << x for x in range(12, 21, 1)])
    max_matches = ("--mm", (10, 1000, 0))
    lazy_matching = ("--lm", (False, True))

    params = [
        levels,
        formats,
        jobs,
        blocks,
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

        if not test[lazy_matching[0]]:
            del test[lazy_matching[0]]

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
    print("Running", test)
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
    ratio = round(100 * enc / orig, 1)

    return [orig, enc, ratio, enc_time, dec_time]


def run_normal_tests(file):
    res = []
    tests = gen_normal_tests()

    for i, test in enumerate(tests):
        print("%s / %s" % (str(i), str(len(tests))))
        test_res = run_test(file, test)
        res += [test, test_res]

    return res

def save_to_csv(data, filename):
    if not data:
        return

    test_dicts = data[0::2]
    test_results = data[1::2]

    fieldnames = []
    for d in test_dicts:
        for k in d.keys():
            if k not in fieldnames:
                fieldnames.append(k)

    result_headers = ["orig_size", "enc_size", "ratio", "enc_time", "dec_time"]
    fieldnames.extend(result_headers)

    with open(filename, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for t_dict, t_res in zip(test_dicts, test_results):
            row = t_dict.copy()
            row["orig_size"] = t_res[0]
            row["enc_size"] = t_res[1]
            row["ratio"] = t_res[2]
            row["enc_time"] = t_res[3]
            row["dec_time"] = t_res[4]
            writer.writerow(row)



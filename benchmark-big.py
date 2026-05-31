import subprocess
import filecmp
import time
import sys
import os
import itertools
import csv

compressor = "./build/lazy"


def encode(src, dest, args):
    try:
        subprocess.run(
            [compressor, "-e", "-i", src, "-o", dest, "-p"] + args, check=True
        )
    except subprocess.CalledProcessError:
        print(f"Encode failed for {src} with args {args}")
        sys.exit(1)


def decode(src, dest, args):
    try:
        subprocess.run(
            [compressor, "-d", "-i", src, "-o", dest, "-p"] + args, check=True
        )
    except subprocess.CalledProcessError:
        print(f"Decode failed for {src} with args {args}")
        sys.exit(1)


def compare(src, dest):
    if not filecmp.cmp(src, dest, shallow=False):
        print(f"Files are not the same: {src} and {dest}")


def gen_normal_tests():
    levels = ("-l", (0,))
    formats = ("-f", ("turbo2",))
    jobs = ("-j", (16, 8, 4, 2, 1))
    divisions = ("-k", (1, 2, 4, 8, 16,))
    block_size = ("--bs", [1 << x for x in [22, 23, 24, 25, 27]])
    window_size = ("--ws", block_size[1])
    future_limit = ("--fl", block_size[1])
    max_matches = ("--mm", (0,))
    lazy_matching = ("--lm", (1, 2,))

    params = [
        levels,
        formats,
        jobs,
        divisions,
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
        if test["--bs"] != test["--ws"] or test["--bs"] != test["--fl"]:
            continue
        if test["-j"] * test["-k"] < 8 or (test["-k"] == 1 and test["-j"] != 16):
            continue
        tests.append(test)
    return tests


def measure(fn, *args):
    before = time.time()
    fn(*args)
    return time.time() - before

def run_test(file, test, cmd_args):
    encoded = "/tmp/encoded_benchmark"
    decoded = "/tmp/decoded_benchmark"

    enc_time = measure(encode, file, encoded, cmd_args)
    dec_time = measure(decode, encoded, decoded, cmd_args)
    compare(file, decoded)

    orig = os.path.getsize(file)
    enc = os.path.getsize(encoded)
    ratio = round(100 * enc / orig, 6) if orig > 0 else 0

    return [orig, enc, ratio, enc_time, dec_time]


def run_all_tests(folder, csv_filename):
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

    config_keys = list(tests[0].keys())
    expected_fieldnames = config_keys + [
        "filename", "orig_size", "enc_size", "ratio", "enc_time", "dec_time"
    ]

    past_dict = {}
    if os.path.exists(csv_filename):
        try:
            with open(csv_filename, "r", newline="") as f:
                reader = csv.DictReader(f)
                if reader.fieldnames and set(expected_fieldnames).issubset(set(reader.fieldnames)):
                    for row in reader:
                        key = (row["filename"], tuple(str(row.get(k)) for k in config_keys))
                        past_dict[key] = {
                            "orig_size": float(row.get("orig_size", 0)),
                            "enc_size": float(row.get("enc_size", 0)),
                            "enc_time": float(row.get("enc_time", 0)),
                            "dec_time": float(row.get("dec_time", 0))
                        }
        except Exception:
            pass

    file_mode = "a" if past_dict else "w"
    total = len(tests) * (len(files) + 1)
    i = 0

    with open(csv_filename, file_mode, newline="") as f:
        writer = csv.DictWriter(f, fieldnames=expected_fieldnames)
        if file_mode == "w":
            writer.writeheader()
            f.flush()

        for test in tests:
            config_tuple = tuple(str(test[k]) for k in config_keys)
            
            # Pre-build argument list once per configuration to avoid repeated serialization
            cmd_args = []
            for a, b in test.items():
                if not isinstance(b, bool):
                    cmd_args.extend([a, str(b)])
                elif b:
                    cmd_args.append(a)

            orig_sum = 0.0
            enc_sum = 0.0
            enc_time_sum = 0.0
            dec_time_sum = 0.0
            count = 0

            for file in files:
                i += 1
                lookup_key = (file, config_tuple)
                
                if lookup_key in past_dict:
                    past_row = past_dict[lookup_key]
                    orig_sum += past_row["orig_size"]
                    enc_sum += past_row["enc_size"]
                    enc_time_sum += past_row["enc_time"]
                    dec_time_sum += past_row["dec_time"]
                    count += 1
                else:
                    print(f"[{i}/{total}] Running file: {file} | config: {test}")
                    orig, enc, ratio, enc_time, dec_time = run_test(file, test, cmd_args)

                    orig_sum += orig
                    enc_sum += enc
                    enc_time_sum += enc_time
                    dec_time_sum += dec_time
                    count += 1

                    row = test.copy()
                    row["filename"] = file
                    row["orig_size"] = orig
                    row["enc_size"] = enc
                    row["ratio"] = ratio
                    row["enc_time"] = enc_time
                    row["dec_time"] = dec_time

                    writer.writerow(row)
                    f.flush()

                    past_dict[lookup_key] = {
                        "orig_size": orig,
                        "enc_size": enc,
                        "enc_time": enc_time,
                        "dec_time": dec_time
                    }
                    time.sleep(0.5)

            i += 1
            avg_lookup_key = ("AVERAGE", config_tuple)
            
            if avg_lookup_key not in past_dict:
                print(f"[{i}/{total}] Computing AVERAGE | config: {test}")
                
                avg_orig = orig_sum / count if count > 0 else 0
                avg_enc = enc_sum / count if count > 0 else 0
                avg_ratio = round(100 * enc_sum / orig_sum, 6) if orig_sum > 0 else 0
                avg_enc_time = enc_time_sum / count if count > 0 else 0
                avg_dec_time = dec_time_sum / count if count > 0 else 0

                row = test.copy()
                row["filename"] = "AVERAGE"
                row["orig_size"] = avg_orig
                row["enc_size"] = avg_enc
                row["ratio"] = avg_ratio
                row["enc_time"] = avg_enc_time
                row["dec_time"] = avg_dec_time

                writer.writerow(row)
                f.flush()
                
                past_dict[avg_lookup_key] = {
                    "orig_size": avg_orig,
                    "enc_size": avg_enc,
                    "enc_time": avg_enc_time,
                    "dec_time": avg_dec_time
                }
                time.sleep(0.5)



if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_folder> <output_csv>")
        sys.exit(1)

    run_all_tests(sys.argv[1], sys.argv[2])

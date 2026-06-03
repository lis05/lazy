#!/usr/bin/env python3

import csv
import filecmp
import os
import shutil
import subprocess
import sys
import time

TMP_ENC = "/tmp/encoded_ct"
TMP_DEC = "/tmp/decoded_ct"
TMP_7Z_DIR = "/tmp/7z_out"
TMP_MISA_DIR = "/tmp/misa77_out"

COMPRESSORS = [
    {
        "name": "lazy",
        "levels": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                   20],
        "compress": lambda f, lvl: [
            "./build/lazy",
            "-e",
            "-i",
            f,
            "-o",
            TMP_ENC,
            "-l",
            str(lvl),
        ]
        + (["-p"] if int(lvl) >= 8 else []),
        "decompress": lambda lvl: [
            "./build/lazy",
            "-d",
            "-i",
            TMP_ENC,
            "-o",
            TMP_DEC,
        ],
    },
    {
        "name": "misa77",
        "levels": [1],
        "compress": lambda f, lvl: ["misa77", "-c", f, "-o", TMP_ENC],
        "decompress": lambda lvl: ["misa77", "-d", TMP_ENC, "-o", TMP_MISA_DIR]
    },
    {
        "name": "gzip",
        "exe": "pigz",
        "levels": list(range(1, 10)),
        "compress": lambda f, lvl: ["pigz", "-p", "16", f"-{lvl}", "-c", f],
        "decompress": lambda lvl: ["pigz", "-p", "16", "-d", "-c", TMP_ENC],
    },
    {
        "name": "bzip2",
        "exe": "pbzip2",
        "levels": list(range(1, 10)),
        "compress": lambda f, lvl: ["pbzip2", "-p16", f"-{lvl}", "-c", f],
        "decompress": lambda lvl: ["pbzip2", "-p16", "-d", "-c", TMP_ENC],
    },
    {
        "name": "bzip3",
        "levels": [1, 2, 4, 8, 16, 32, 64, 128],
        "compress": lambda f, lvl: ["bzip3", "-j", "16", "-b", str(lvl), "-c", f],
        "decompress": lambda lvl: ["bzip3", "-j", "16", "-d", "-c", TMP_ENC],
    },
    {
        "name": "zstd",
        "levels": list(range(1, 23)),
        "compress": lambda f, lvl: (
            ["zstd", "-T16", "--ultra", f"-{lvl}", "-c", f]
            if lvl >= 20
            else ["zstd", "-T16", f"-{lvl}", "-c", f]
        ),
        "decompress": lambda lvl: ["zstd", "-T16", "-d", "-c", TMP_ENC],
    },
    {
        "name": "brotli",
        "levels": list(range(0, 12)),
        "compress": lambda f, lvl: ["brotli", "-q", str(lvl), "-c", f],
        "decompress": lambda lvl: ["brotli", "-d", "-c", TMP_ENC],
    },
    {
        "name": "xz",
        "levels": list(range(0, 10)),
        "compress": lambda f, lvl: ["xz", "-T16", f"-{lvl}", "-c", f],
        "decompress": lambda lvl: ["xz", "-T16", "-d", "-c", TMP_ENC],
    },
    {
        "name": "7z",
        "levels": [1, 3, 5, 7, 9],
        "compress": lambda f, lvl: ["7z", "a", "-bd", f"-mx={lvl}", "-mmt=16", TMP_ENC + ".7z", f],
        "decompress": lambda lvl: [
            "7z",
            "x",
            "-y",
            "-mmt=16",
            TMP_ENC + ".7z",
            f"-o{TMP_7Z_DIR}",
        ],
        "special_7z": True,
    },
    {
        "name": "lz4",
        "levels": list(range(1, 13)),
        "compress": lambda f, lvl: ["lz4", f"-{lvl}", f, TMP_ENC],
        "decompress": lambda lvl: ["lz4", "-d", TMP_ENC, TMP_DEC],
    },
    {
        "name": "lzma",
        "levels": list(range(1, 10)),
        "compress": lambda f, lvl: ["lzma", f"-{lvl}", "-c", f],
        "decompress": lambda lvl: ["lzma", "-d", "-c", TMP_ENC],
    },
]


def cleanup():
    for p in [TMP_ENC, TMP_DEC, TMP_ENC + ".7z"]:
        try:
            os.remove(p)
        except FileNotFoundError:
            pass

    shutil.rmtree(TMP_7Z_DIR, ignore_errors=True)
    shutil.rmtree(TMP_MISA_DIR, ignore_errors=True)


def run(cmd, out=None):
    start = time.perf_counter()

    if out:
        with open(out, "wb") as f:
            subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    else:
        subprocess.run(cmd, stderr=subprocess.DEVNULL, check=True)

    return time.perf_counter() - start


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

    expected_fieldnames = [
        "filename",
        "compressor",
        "level",
        "orig_size",
        "enc_size",
        "ratio_percent",
        "enc_time",
        "dec_time",
    ]

    past_dict = {}
    if os.path.exists(csv_filename):
        try:
            with open(csv_filename, "r", newline="") as f:
                reader = csv.DictReader(f)
                if reader.fieldnames and set(expected_fieldnames).issubset(set(reader.fieldnames)):
                    for row in reader:
                        key = (row["filename"], row["compressor"], str(row["level"]))
                        past_dict[key] = {
                            "orig_size": float(row.get("orig_size", 0)),
                            "enc_size": float(row.get("enc_size", 0)),
                            "enc_time": float(row.get("enc_time", 0)),
                            "dec_time": float(row.get("dec_time", 0)),
                        }
        except Exception:
            pass

    file_mode = "a" if past_dict else "w"

    with open(csv_filename, file_mode, newline="") as f:
        writer = csv.DictWriter(f, fieldnames=expected_fieldnames)
        if file_mode == "w":
            writer.writeheader()
            f.flush()

        for c in COMPRESSORS:
            name = c["name"]
            exe_name = c.get("exe", name)

            if shutil.which(exe_name) is None and name not in ["lazy", "misa77", "7z", "bzip2"]:
                print(f"skip {name}")
                continue

            for lvl in c["levels"]:
                orig_sum = 0.0
                enc_sum = 0.0
                enc_time_sum = 0.0
                dec_time_sum = 0.0
                count = 0

                for file in files:
                    lookup_key = (file, name, str(lvl))

                    if lookup_key in past_dict:
                        past_row = past_dict[lookup_key]
                        orig_sum += past_row["orig_size"]
                        enc_sum += past_row["enc_size"]
                        enc_time_sum += past_row["enc_time"]
                        dec_time_sum += past_row["dec_time"]
                        count += 1
                        continue

                    print(f"Running: {file} | {name} lvl={lvl}")
                    cleanup()

                    try:
                        orig_size = os.path.getsize(file)
                        enc_cmd = c["compress"](file, lvl)

                        if name in ["gzip", "bzip2", "bzip3", "zstd", "brotli", "xz", "lzma"]:
                            enc_t = run(enc_cmd, out=TMP_ENC)
                        elif c.get("special_7z"):
                            enc_t = run(enc_cmd)
                        else:
                            enc_t = run(enc_cmd)

                        archive = TMP_ENC + ".7z" if c.get("special_7z") else TMP_ENC

                        if not os.path.exists(archive):
                            raise RuntimeError("missing compressed output")

                        enc_size = os.path.getsize(archive)
                        ratio = (enc_size / orig_size) * 100.0 if orig_size > 0 else 0

                        dec_cmd = c["decompress"](lvl)

                        if c.get("special_7z"):
                            dec_t = run(dec_cmd)

                            extracted = None
                            for root, _, extracted_files in os.walk(TMP_7Z_DIR):
                                for xf in extracted_files:
                                    extracted = os.path.join(root, xf)
                                    break

                            if not extracted:
                                raise RuntimeError("7z extraction failed")

                            shutil.copyfile(extracted, TMP_DEC)

                        elif name == "misa77":
                            dec_t = run(dec_cmd)

                            extracted = None
                            for root, _, extracted_files in os.walk(TMP_MISA_DIR):
                                for xf in extracted_files:
                                    extracted = os.path.join(root, xf)
                                    break

                            if not extracted:
                                raise RuntimeError("misa77 extraction failed")

                            shutil.copyfile(extracted, TMP_DEC)

                        elif name in ["gzip", "bzip2", "bzip3", "zstd", "brotli", "xz", "lzma"]:
                            dec_t = run(dec_cmd, out=TMP_DEC)
                        else:
                            dec_t = run(dec_cmd)

                        if not filecmp.cmp(file, TMP_DEC, shallow=False):
                            print(f"verification failed for {file} with {name} lvl={lvl}")
                            continue

                        orig_sum += orig_size
                        enc_sum += enc_size
                        enc_time_sum += enc_t
                        dec_time_sum += dec_t
                        count += 1

                        row = {
                            "filename": file,
                            "compressor": name,
                            "level": lvl,
                            "orig_size": orig_size,
                            "enc_size": enc_size,
                            "ratio_percent": ratio,
                            "enc_time": enc_t,
                            "dec_time": dec_t,
                        }

                        writer.writerow(row)
                        f.flush()

                        past_dict[lookup_key] = {
                            "orig_size": orig_size,
                            "enc_size": enc_size,
                            "enc_time": enc_t,
                            "dec_time": dec_t,
                        }

                    except Exception as e:
                        print(f"FAILED {name} lvl={lvl} on {file}: {e}")

                    finally:
                        cleanup()

                if count > 0:
                    avg_lookup_key = ("AVERAGE", name, str(lvl))

                    if avg_lookup_key not in past_dict:
                        print(f"Computing AVERAGE | {name} lvl={lvl}")

                        avg_orig = orig_sum / count
                        avg_enc = enc_sum / count
                        avg_ratio = (enc_sum / orig_sum) * 100.0 if orig_sum > 0 else 0
                        avg_enc_time = enc_time_sum / count
                        avg_dec_time = dec_time_sum / count

                        row = {
                            "filename": "AVERAGE",
                            "compressor": name,
                            "level": lvl,
                            "orig_size": avg_orig,
                            "enc_size": avg_enc,
                            "ratio_percent": avg_ratio,
                            "enc_time": avg_enc_time,
                            "dec_time": avg_dec_time,
                        }

                        writer.writerow(row)
                        f.flush()

                        past_dict[avg_lookup_key] = {
                            "orig_size": avg_orig,
                            "enc_size": avg_enc,
                            "enc_time": avg_enc_time,
                            "dec_time": avg_dec_time,
                        }


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_folder> <output_csv>")
        sys.exit(1)

    run_all_tests(sys.argv[1], sys.argv[2])


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

if len(sys.argv) < 2:
    print(f"usage: {sys.argv[0]} <file1> [file2] ...")
    sys.exit(1)

INPUT_FILES = []
file_names = []
for arg in sys.argv[1:]:
    path = os.path.abspath(arg)
    if not os.path.isfile(path):
        print(f"error: file does not exist: {path}")
        sys.exit(1)
    INPUT_FILES.append(path)
    file_names.append(os.path.basename(path))

CSV_FILE = f"comparisons/test-{'-'.join(file_names)}.csv"

COMPRESSORS = [
    {
        "name": "lz77",
        "levels": [1, 2, 3, 4, 5, 6, 7, 8],

        "compress": lambda f, lvl: [
            "./build/lz77",
            "-e",
            "-i", f,
            "-o", TMP_ENC,
            "-l", str(lvl),
        ] + (["-p"] if int(lvl) >= 8 else []),

        "decompress": lambda lvl: [
            "./build/lz77",
            "-d",
            "-i",
            TMP_ENC,
            "-o",
            TMP_DEC,
        ],
    },
    {
        "name": "gzip",
        # Full levels: 1 to 9
        "levels": list(range(1, 10)),
        "compress": lambda f, lvl: ["gzip", f"-{lvl}", "-c", f],
        "decompress": lambda lvl: ["gzip", "-d", "-c", TMP_ENC],
    },
    {
        "name": "zstd",
        # Fast + normal + ultra range
        "levels": (list(range(1, 20)) + [20, 21, 22]),  # 1..19  # ultra levels
        "compress": lambda f, lvl: (["zstd", f"-{lvl}", "-c", f]),
        "decompress": lambda lvl: ["zstd", "-d", "-c", TMP_ENC],
    },
    {
        "name": "brotli",
        # Full levels: 0 to 11
        "levels": list(range(0, 12)),
        "compress": lambda f, lvl: ["brotli", "-q", str(lvl), "-c", f],
        "decompress": lambda lvl: ["brotli", "-d", "-c", TMP_ENC],
    },
    {
        "name": "xz",
        # Full levels: 0 to 9
        "levels": list(range(0, 10)),
        "compress": lambda f, lvl: ["xz", f"-{lvl}", "-c", f],
        "decompress": lambda lvl: ["xz", "-d", "-c", TMP_ENC],
    },
    {
        "name": "7z",
        # Full levels: 0, 1, 3, 5, 7, 9
        "levels": [1, 3, 5, 7, 9],
        "compress": lambda f, lvl: ["7z", "a", "-bd", f"-mx={lvl}", TMP_ENC + ".7z", f],
        "decompress": lambda lvl: [
            "7z",
            "x",
            "-y",
            TMP_ENC + ".7z",
            f"-o{TMP_7Z_DIR}",
        ],
        "special_7z": True,
    },
    {
        "name": "lz4",
        # Full levels: 1 to 12 (1-9 normal, 10-12 high compression)
        "levels": [e for e in range(1, 12)],
        "compress": lambda f, lvl: ["lz4", f"-{lvl}", f, TMP_ENC],
        "decompress": lambda lvl: ["lz4", "-d", TMP_ENC, TMP_DEC],
    },
    {
        "name": "snappy",
        # Full levels: None (Snappy does not have tunable compression levels)
        "levels": ["default"],
        "compress": lambda f, lvl: ["snzip", "-c", f],
        "decompress": lambda lvl: ["snzip", "-d", "-c", TMP_ENC],
    },
]


def cleanup():
    for p in [TMP_ENC, TMP_DEC, TMP_ENC + ".7z"]:
        try:
            os.remove(p)
        except FileNotFoundError:
            pass

    shutil.rmtree(TMP_7Z_DIR, ignore_errors=True)


def run(cmd, out=None):
    start = time.perf_counter()

    if out:
        with open(out, "wb") as f:
            subprocess.run(cmd, stdout=f, stderr=subprocess.DEVNULL, check=True)
    else:
        subprocess.run(
            cmd, stderr=subprocess.DEVNULL, check=True
        )

    return time.perf_counter() - start


results = []

for file in INPUT_FILES:
    fname = os.path.basename(file)
    orig_size = os.path.getsize(file)

    print(f"\n=== {fname} ===")

    for c in COMPRESSORS:
        name = c["name"]

        if shutil.which(name) is None and name not in ["lz77", "7z", "snappy"]:
            print(f"skip {name}")
            continue

        for lvl in c["levels"]:
            print(f"{name} lvl={lvl}")
            cleanup()

            try:
                enc_cmd = c["compress"](file, lvl)

                # compression
                if name in ["gzip", "zstd", "brotli", "xz", "snappy"]:
                    enc_t = run(enc_cmd, out=TMP_ENC)
                elif c.get("special_7z"):
                    enc_t = run(enc_cmd)
                else:
                    enc_t = run(enc_cmd)

                archive = TMP_ENC + ".7z" if c.get("special_7z") else TMP_ENC

                if not os.path.exists(archive):
                    raise RuntimeError("missing compressed output")

                enc_size = os.path.getsize(archive)
                ratio = (enc_size / orig_size) * 100.0

                # decompression
                dec_cmd = c["decompress"](lvl)

                if c.get("special_7z"):
                    dec_t = run(dec_cmd)

                    extracted = None
                    for root, _, files in os.walk(TMP_7Z_DIR):
                        for f in files:
                            extracted = os.path.join(root, f)
                            break

                    if not extracted:
                        raise RuntimeError("7z extraction failed")

                    shutil.copyfile(extracted, TMP_DEC)

                elif name in ["gzip", "zstd", "brotli", "xz", "snappy"]:
                    dec_t = run(dec_cmd, out=TMP_DEC)
                else:
                    dec_t = run(dec_cmd)

                # correctness check
                if not filecmp.cmp(file, TMP_DEC, shallow=False):
                    print("verification failed")
                    continue

                results.append(
                    {
                        "filename": fname,
                        "compressor": name,
                        "level": lvl,
                        "enc_time": enc_t,
                        "dec_time": dec_t,
                        "enc_size": enc_size,
                        "ratio_percent": ratio,
                    }
                )

            except Exception as e:
                print(f"FAILED {name} lvl={lvl}: {e}")

            finally:
                cleanup()


with open(CSV_FILE, "w", newline="") as f:
    writer = csv.DictWriter(
        f,
        fieldnames=[
            "filename",
            "compressor",
            "level",
            "enc_time",
            "dec_time",
            "enc_size",
            "ratio_percent",
        ],
    )
    writer.writeheader()
    writer.writerows(results)

print(f"\ndone -> {CSV_FILE}")


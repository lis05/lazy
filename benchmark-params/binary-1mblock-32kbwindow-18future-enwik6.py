import csv
import os

# Set output filename based on this script's name (.py -> .csv)
SCRIPT_NAME = os.path.basename(__file__)
OUTPUT_FILE = os.path.splitext(SCRIPT_NAME)[0] + ".csv"

HEADER = [
    "file",
    "format",
    "block_size",
    "window_size",
    "future_limit",
    "max_matches",
    "len3_dist_bits",
    "len4_dist_bits",
    "len5_dist_bits",
    "len6_dist_bits",
    "len7_dist_bits",
    "lenx_dist_bits",
    "lenx_len_bits",
]

FILE_VAL = "big_files/enwik6"
FORMAT_VAL = "binary"
BLOCK_SIZE = 1048576
WINDOW_SIZE = 32768
FUTURE_LIMIT = 18
MAX_MATCHES = 3
LENX_DIST_BITS = 15
LENX_LEN_BITS = 4

BIT_OPTIONS = [4, 6, 8, 10, 12, 15]

with open(OUTPUT_FILE, mode="w", newline="\n") as f:
    writer = csv.writer(f)
    writer.writerow(HEADER)

    for b3 in BIT_OPTIONS:
        for b4 in BIT_OPTIONS:
            if b4 < b3:
                continue
            for b5 in BIT_OPTIONS:
                if b5 < b4:
                    continue
                for b6 in BIT_OPTIONS:
                    if b6 < b5:
                        continue
                    for b7 in BIT_OPTIONS:
                        if b7 < b6:
                            continue

                        row = [
                            FILE_VAL,
                            FORMAT_VAL,
                            BLOCK_SIZE,
                            WINDOW_SIZE,
                            FUTURE_LIMIT,
                            MAX_MATCHES,
                            b3,
                            b4,
                            b5,
                            b6,
                            b7,
                            LENX_DIST_BITS,
                            LENX_LEN_BITS,
                        ]
                        writer.writerow(row)

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
FORMAT_VAL = "fse"
MAX_MATCHES = 3

# Static defaults for unused distribution fields under FSE mode
DUMMY_VAL = 0

# Define parameter ranges (in bytes)
BLOCK_SIZES = [16384, 32768, 65536, 131072, 262144, 524288, 1048576]
WINDOW_SIZES = [2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576]
FUTURE_LIMITS = [3, 4, 8, 16, 32, 64, 128, 256, 258]

with open(OUTPUT_FILE, mode="w", newline="\n") as f:
    writer = csv.writer(f)
    writer.writerow(HEADER)

    for block_size in BLOCK_SIZES:
        for window_size in WINDOW_SIZES:
            # Constraint: window size must not be larger than block size
            if window_size > block_size:
                continue
            for future_limit in FUTURE_LIMITS:
                if future_limit > window_size:
                    continue
                row = [
                    FILE_VAL,
                    FORMAT_VAL,
                    block_size,
                    window_size,
                    future_limit,
                    MAX_MATCHES,
                    DUMMY_VAL,
                    DUMMY_VAL,
                    DUMMY_VAL,
                    DUMMY_VAL,
                    DUMMY_VAL,
                    DUMMY_VAL,
                    DUMMY_VAL,
                ]
                writer.writerow(row)

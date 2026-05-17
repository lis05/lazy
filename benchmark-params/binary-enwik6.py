import csv
import itertools
import math
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
MAX_MATCHES = 3

BLOCK_SIZES = [32768, 131072, 524288, 1048576]
WINDOW_SIZES = [4096, 16384, 32768, 131072, 262144]
FUTURE_LIMITS = [4, 8, 18, 32, 64, 128, 256, 258]

BIT_OPTIONS = [6, 12, 15]

with open(OUTPUT_FILE, mode="w", newline="\n") as f:
    writer = csv.writer(f)
    writer.writerow(HEADER)

    # Generate configurations for block, window, and future size parameters
    for block_size in BLOCK_SIZES:
        for window_size in WINDOW_SIZES:
            for future_limit in FUTURE_LIMITS:
                if window_size > block_size or future_limit > window_size:
                    continue

                # LENX_DIST_BITS must cover the entire window size
                lenx_dist_bits = math.ceil(math.log2(window_size))

                # LENX_LEN_BITS must be at least floor(log2(future_limit - 2))
                lenx_len_bits = math.ceil(math.log2(future_limit - 2))

                # Generate combinations for bit arrays (non-decreasing sequence b3 <= b4 <= b5 <= b6 <= b7)
                for combo in itertools.combinations_with_replacement(BIT_OPTIONS, 5):
                    b3, b4, b5, b6, b7 = combo

                    row = [
                        FILE_VAL,
                        FORMAT_VAL,
                        block_size,
                        window_size,
                        future_limit,
                        MAX_MATCHES,
                        b3,
                        b4,
                        b5,
                        b6,
                        b7,
                        lenx_dist_bits,
                        lenx_len_bits,
                    ]
                    writer.writerow(row)

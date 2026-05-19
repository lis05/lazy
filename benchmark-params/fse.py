import csv
import os

# Set output filename based on this script's name (.py -> .csv)
SCRIPT_NAME = os.path.basename(__file__)
OUTPUT_FILE = os.path.splitext(SCRIPT_NAME)[0] + ".csv"

HEADER = [
    "format",
    "block_size",
    "window_size",
    "future_limit",
    "max_matches",
]

FILE_VAL = "big_files/enwik6"
FORMAT_VAL = "fse"
# Try a range of max_matches values to explore matching limits
MAX_MATCHES_LIST = [1, 2, 4, 16, 128, 1024, 999999999]


# Define parameter ranges (in bytes)
BLOCK_SIZES = [32768, 131072, 524288, 1048576]
WINDOW_SIZES = [4096, 16384, 32768, 131072, 262144]
FUTURE_LIMITS = [4, 8, 18, 32, 64, 128, 256, 258]

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
                for max_matches in MAX_MATCHES_LIST:
                    row = [
                        FORMAT_VAL,
                        block_size,
                        window_size,
                        future_limit,
                        max_matches,
                    ]
                    writer.writerow(row)

#!/bin/bash

# Parse arguments
CLEAN_MODE=0
FOLDER=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --clean) CLEAN_MODE=1 ;;
        *) FOLDER="$1" ;;
    esac
    shift
done

DEC_ITERATIONS=5 # Number of decompression iterations to run for averaging

if [ -z "$FOLDER" ] || [ ! -d "$FOLDER" ]; then
    echo "Error: Reference a valid dataset folder."
    echo "Usage: $0 <dataset_folder> [--clean]"
    exit 1
fi

# Extract dataset folder name
DATASET_NAME=$(basename "$FOLDER")

OUTPUT_DIR=".benchmarks"
CACHE_DIR=".benchmarks-cache"

# Handle dataset-specific clean logic
if [ "$CLEAN_MODE" -eq 1 ]; then
    echo "Cleaning benchmark cache for dataset: $DATASET_NAME..."
    if [ -d "$CACHE_DIR" ]; then
        for cache_file in "$CACHE_DIR"/*.cache; do
            if [ -f "$cache_file" ]; then
                # Extract the first column (dataset name) from the cache row to match
                first_word=$(awk '{print $1; exit}' "$cache_file")
                if [ "$first_word" == "$DATASET_NAME" ]; then
                    rm -f "$cache_file"
                fi
            fi
        done
    fi
    echo "Clean complete."
    exit 0
fi

# Create required directories
mkdir -p "$OUTPUT_DIR" "$CACHE_DIR"

# Define compressors to benchmark
# Format: "Display Name|Compression Command|Decompression Command"
COMPRESSORS=(
    "zstd -22|zstd -22 -T0 --ultra INPUT -o OUTPUT|zstd -d INPUT -o OUTPUT"
    "zstd -18|zstd -18 -T0 INPUT -o OUTPUT|zstd -d INPUT -o OUTPUT"
    "brotli -q9|brotli -q 9 INPUT -o OUTPUT|brotli -d INPUT -o OUTPUT"
    "brotli -q11|brotli -q 11 INPUT -o OUTPUT|brotli -d INPUT -o OUTPUT"
    "brotli -q11l30|brotli -q 11 --large_window=30 INPUT -o OUTPUT|brotli -d INPUT -o OUTPUT"
    "lzmpo 0a1rs0|./build/lzmpo -0 -a1 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 0a3rs0|./build/lzmpo -0 -a3 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 2a1rs0|./build/lzmpo -2 -a1 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 2a3rs0|./build/lzmpo -2 -a3 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 4a1rs0|./build/lzmpo -4 -a1 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 4a3rs0|./build/lzmpo -4 -a3 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 6a1rs0|./build/lzmpo -6 -a1 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 6a3rs0|./build/lzmpo -6 -a3 --rans_static0 -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
    "lzmpo 6a3tr|./build/lzmpo -6 -a3 --turborc -e -i INPUT -o OUTPUT|./build/lzmpo -d -i INPUT -o OUTPUT"
)

# Array to buffer text output rows for printing at the end
declare -a TABLE_ROWS

for comp_entry in "${COMPRESSORS[@]}"; do
    IFS='|' read -r COMP_NAME COMP_CMD DEC_CMD <<<"$comp_entry"

    # Identify the binary executable and calculate its checksum
    EXE_NAME=$(echo "$COMP_CMD" | awk '{print $1}')
    if [[ "$EXE_NAME" == /* ]] || [[ "$EXE_NAME" == .* ]]; then
        EXE_PATH=$(realpath "$EXE_NAME" 2>/dev/null)
    else
        EXE_PATH=$(command -v "$EXE_NAME" 2>/dev/null)
    fi

    if [ -f "$EXE_PATH" ]; then
        EXE_HASH=$(sha256sum "$EXE_PATH" | awk '{print $1}')
    else
        EXE_HASH="unknown"
    fi

    # Generate a unique cache key based on binary hash, parameters, dataset name, and iterations
    CACHE_KEY_STR="${DATASET_NAME}|${EXE_HASH}|${COMP_CMD}|${DEC_CMD}|${DEC_ITERATIONS}"
    CACHE_KEY_HASH=$(echo -n "$CACHE_KEY_STR" | sha256sum | awk '{print $1}')
    
    # Restore original cache filename structure to retain compatibility with previously generated cache files
    CACHE_FILE="${CACHE_DIR}/${CACHE_KEY_HASH}.cache"

    # Check for hit in the caching system
    if [ -f "$CACHE_FILE" ]; then
        echo "Running engine: $COMP_NAME (Cached)"
        ROW=$(cat "$CACHE_FILE")
        TABLE_ROWS+=("$ROW")
        continue
    fi

    TOTAL_ORIG_SIZE=0
    TOTAL_COMP_SIZE=0
    TOTAL_COMP_TIME=0
    MAX_COMP_RAM=0

    TOTAL_DEC_TIME=0
    MAX_DEC_RAM=0
    EXEC_FAILED=0

    echo "Running engine: $COMP_NAME"

    # Iterate through all files in the target directory
    for FILE in "$FOLDER"/*; do
        if [ ! -f "$FILE" ]; then continue; fi

        FILE_NAME=$(basename "$FILE")
        COMPRESSED_FILE="${OUTPUT_DIR}/${FILE_NAME}.comp"
        DECOMPRESSED_FILE="${OUTPUT_DIR}/${FILE_NAME}.decomp"

        # Clear any existing files from previous runs to avoid stale data corruption
        rm -f "$COMPRESSED_FILE" "$DECOMPRESSED_FILE"

        # Get original file size
        ORIG_SIZE=$(ls -l "$FILE" | awk '{print $5}')
        TOTAL_ORIG_SIZE=$((TOTAL_ORIG_SIZE + ORIG_SIZE))

        # --- Compression Phase ---
        echo "  [Comp] $FILE_NAME"
        RUN_COMP_CMD=${COMP_CMD/INPUT/"$FILE"}
        RUN_COMP_CMD=${RUN_COMP_CMD/OUTPUT/"$COMPRESSED_FILE"}

        # Track wall time (%e) and Max RSS (%M)
        COMP_LOG=$({ /usr/bin/time -f "BENCHMARK: %e %M" $RUN_COMP_CMD; } 2>&1 >/dev/null)

        # Verify if command executed and output file exists
        if [ ! -f "$COMPRESSED_FILE" ] || [ ! -s "$COMPRESSED_FILE" ]; then
            EXEC_FAILED=1
            echo "  -> Compression Failed"
            break
        fi

        COMP_TIME=$(echo "$COMP_LOG" | awk '/BENCHMARK:/ {print $2}')
        COMP_RAM=$(echo "$COMP_LOG" | awk '/BENCHMARK:/ {print $3}')

        TOTAL_COMP_TIME=$(awk -v t1="$TOTAL_COMP_TIME" -v t2="$COMP_TIME" 'BEGIN {print t1 + t2}')
        if [ -n "$COMP_RAM" ] && [ "$COMP_RAM" -gt "$MAX_COMP_RAM" ]; then
            MAX_COMP_RAM=$COMP_RAM
        fi

        # Get compressed file size
        COMP_SIZE=$(ls -l "$COMPRESSED_FILE" | awk '{print $5}')
        TOTAL_COMP_SIZE=$((TOTAL_COMP_SIZE + COMP_SIZE))

        # --- Decompression Phase (Multi-run Average) ---
        RUN_DEC_CMD=${DEC_CMD/INPUT/"$COMPRESSED_FILE"}
        RUN_DEC_CMD=${RUN_DEC_CMD/OUTPUT/"$DECOMPRESSED_FILE"}

        FILE_DEC_TIME_SUM=0
        for ((i = 1; i <= DEC_ITERATIONS; i++)); do
            echo "  [Decomp] $FILE_NAME (Iter $i/$DEC_ITERATIONS)"
            rm -f "$DECOMPRESSED_FILE"
            DEC_LOG=$({ /usr/bin/time -f "BENCHMARK: %e %M" $RUN_DEC_CMD; } 2>&1 >/dev/null)

            if [ ! -f "$DECOMPRESSED_FILE" ]; then
                EXEC_FAILED=1
                echo "  -> Decompression Failed"
                break 2
            fi

            DEC_TIME=$(echo "$DEC_LOG" | awk '/BENCHMARK:/ {print $2}')
            DEC_RAM=$(echo "$DEC_LOG" | awk '/BENCHMARK:/ {print $3}')

            FILE_DEC_TIME_SUM=$(awk -v t1="$FILE_DEC_TIME_SUM" -v t2="$DEC_TIME" 'BEGIN {print t1 + t2}')
            if [ -n "$DEC_RAM" ] && [ "$DEC_RAM" -gt "$MAX_DEC_RAM" ]; then
                MAX_DEC_RAM=$DEC_RAM
            fi
        done

        # Aggregate average decompression speed for the current file
        AVG_FILE_DEC_TIME=$(awk -v sum="$FILE_DEC_TIME_SUM" -v it="$DEC_ITERATIONS" 'BEGIN {print sum / it}')
        TOTAL_DEC_TIME=$(awk -v t1="$TOTAL_DEC_TIME" -v t2="$AVG_FILE_DEC_TIME" 'BEGIN {print t1 + t2}')

        # Clean up decompression output to save space
        rm -f "$DECOMPRESSED_FILE"
    done

    # --- Combined Performance Metrics Calculations ---
    if [ "$EXEC_FAILED" -eq 1 ]; then
        ROW=$(printf "%-12s | %-20s | %-57s" "$DATASET_NAME" "$COMP_NAME" "FAILED")
        TABLE_ROWS+=("$ROW")
    elif [ "$TOTAL_ORIG_SIZE" -gt 0 ]; then
        RATIO=$(awk -v c="$TOTAL_COMP_SIZE" -v o="$TOTAL_ORIG_SIZE" 'BEGIN {printf "%.2f", (c/o)*100}')
        TOTAL_ORIG_MB=$(awk -v o="$TOTAL_ORIG_SIZE" 'BEGIN {print o / 1048576}')
        COMP_SPEED=$(awk -v size="$TOTAL_ORIG_MB" -v time="$TOTAL_COMP_TIME" 'BEGIN {printf "%.2f", (time > 0 ? size / time : 0)}')
        DEC_SPEED=$(awk -v size="$TOTAL_ORIG_MB" -v time="$TOTAL_DEC_TIME" 'BEGIN {printf "%.2f", (time > 0 ? size / time : 0)}')
        COMP_RAM_MB=$(awk -v k="$MAX_COMP_RAM" 'BEGIN {printf "%.2f", k/1024}')
        DEC_RAM_MB=$(awk -v k="$MAX_DEC_RAM" 'BEGIN {printf "%.2f", k/1024}')

        ROW=$(printf "%-12s | %-20s | %-7s%% | %-11s | %-9s | %-11s | %-9s" \
            "$DATASET_NAME" "$COMP_NAME" "$RATIO" "$COMP_SPEED" "$COMP_RAM_MB" "$DEC_SPEED" "$DEC_RAM_MB")

        # Save output row to cache file system
        echo "$ROW" >"$CACHE_FILE"
        TABLE_ROWS+=("$ROW")
    else
        ROW=$(printf "%-12s | %-20s | %-57s" "$DATASET_NAME" "$COMP_NAME" "EMPTY DATASET")
        TABLE_ROWS+=("$ROW")
    fi
done

# --- Final Consolidated Report ---
echo ""
echo "==============================================================================================="
printf "%-12s | %-20s | %-8s | %-11s | %-9s | %-11s | %-9s\n" \
    "Dataset" "Compressor" "Ratio" "C.MB/s" "C.RAM(MB)" "D.MB/s" "D.RAM(MB)"
echo "-----------------------------------------------------------------------------------------------"
# Sort elements numerically (-n) using '|' as the field delimiter (-t) on the third column (-k3,3)
printf "%s\n" "${TABLE_ROWS[@]}" | sort -t'|' -k3,3n
echo "==============================================================================================="

#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Usage: $0 <directory>"
    exit 1
fi

TARGET_DIR="$1"
LZMPO_BIN="./build/lzmpo"

# Configure your custom lzmpo flags here (-k 16 assigns 16 workers)
LZMPO_ENC_FLAGS="--max -k16 --pl 6,8,12,16,20,24 --mm 200"
LZMPO_DEC_FLAGS="--max"

# Create a temporary directory for all output and time files
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Print CSV header
echo "compressor | file | ratio in % | compression time | decompression time | compression memory in MB | decompression memory in MB"

# Find and iterate over all files in the target directory, excluding any existing compressed artifacts
find "$TARGET_DIR" -type f ! -name "*.brotli" ! -name "*.xz" ! -name "*.zstd" ! -name "*.lzmpo" ! -name "*.dec" | while read -r FILE; do
    ORIG_SIZE=$(stat -c %s "$FILE" 2>/dev/null)

    # Skip if stat failed or file is empty to avoid bash syntax errors and division by zero
    if [ -z "$ORIG_SIZE" ] || [ "$ORIG_SIZE" -eq 0 ]; then
        continue
    fi

    BASENAME=$(basename "$FILE")

    for COMP in "brotli" "xz" "zstd" "lzmpo"; do
        COMP_FILE="${TMP_DIR}/${BASENAME}.${COMP}"
        DECOMP_FILE="${TMP_DIR}/${BASENAME}.${COMP}.dec"
        ENC_TIME_FILE="${TMP_DIR}/enc_time.tmp"
        DEC_TIME_FILE="${TMP_DIR}/dec_time.tmp"

        case "$COMP" in
            "brotli")
                /usr/bin/time -o "$ENC_TIME_FILE" -f "%e %M" brotli -q 11 -c "$FILE" >"$COMP_FILE" 2>/dev/null
                /usr/bin/time -o "$DEC_TIME_FILE" -f "%e %M" brotli -d -c "$COMP_FILE" >"$DECOMP_FILE" 2>/dev/null
                ;;
            "xz")
                /usr/bin/time -o "$ENC_TIME_FILE" -f "%e %M" xz -9 -T16 -c "$FILE" >"$COMP_FILE" 2>/dev/null
                /usr/bin/time -o "$DEC_TIME_FILE" -f "%e %M" xz -d -T16 -c "$COMP_FILE" >"$DECOMP_FILE" 2>/dev/null
                ;;
            "zstd")
                /usr/bin/time -o "$ENC_TIME_FILE" -f "%e %M" zstd --ultra -22 -T16 -c -q "$FILE" >"$COMP_FILE" 2>/dev/null
                /usr/bin/time -o "$DEC_TIME_FILE" -f "%e %M" zstd -d -T16 -c -q "$COMP_FILE" >"$DECOMP_FILE" 2>/dev/null
                ;;
            "lzmpo")
                FLAGS=$LZMPO_ENC_FLAGS
                if [ "$BASENAME" == "enwik9" ]; then
                    FLAGS="$FLAGS -b32"
                fi
                /usr/bin/time -o "$ENC_TIME_FILE" -f "%e %M" "$LZMPO_BIN" -e -i "$FILE" -o "$COMP_FILE" $FLAGS >/dev/null 2>/dev/null
                /usr/bin/time -o "$DEC_TIME_FILE" -f "%e %M" "$LZMPO_BIN" -d -i "$COMP_FILE" -o "$DECOMP_FILE" $LZMPO_DEC_FLAGS >/dev/null 2>/dev/null
                ;;
        esac

        # Safely read /usr/bin/time outputs, validating file existence
        ENC_TIME=0.00
        ENC_MEM_KB=0
        if [ -f "$ENC_TIME_FILE" ]; then
            read -r ENC_TIME ENC_MEM_KB <"$ENC_TIME_FILE"
        fi

        DEC_TIME=0.00
        DEC_MEM_KB=0
        if [ -f "$DEC_TIME_FILE" ]; then
            read -r DEC_TIME DEC_MEM_KB <"$DEC_TIME_FILE"
        fi

        # Handle formatting edge cases if 'time' output is mangled
        [[ ! "$ENC_MEM_KB" =~ ^[0-9]+$ ]] && ENC_MEM_KB=0
        [[ ! "$DEC_MEM_KB" =~ ^[0-9]+$ ]] && DEC_MEM_KB=0

        # Safely calculate sizes and ratios
        COMP_SIZE=$(stat -c %s "$COMP_FILE" 2>/dev/null)
        if [ -n "$COMP_SIZE" ]; then
            RATIO=$(awk "BEGIN {printf \"%.2f\", ($COMP_SIZE / $ORIG_SIZE) * 100}")
        else
            RATIO="ERR"
        fi

        # Convert Kilobytes (from %M) to Megabytes
        ENC_MEM_MB=$(awk "BEGIN {printf \"%.2f\", $ENC_MEM_KB / 1024}")
        DEC_MEM_MB=$(awk "BEGIN {printf \"%.2f\", $DEC_MEM_KB / 1024}")

        # Print formatted row
        echo "$COMP | $BASENAME | $RATIO | $ENC_TIME | $DEC_TIME | $ENC_MEM_MB | $DEC_MEM_MB"

        # Cleanup temporary artifacts for the current iteration
        rm -f "$COMP_FILE" "$DECOMP_FILE" "$ENC_TIME_FILE" "$DEC_TIME_FILE"
    done
done

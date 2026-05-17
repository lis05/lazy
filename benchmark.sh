#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <params_file>"
    exit 1
fi

PARAMS_FILE="$1"

if [ ! -f "$PARAMS_FILE" ]; then
    echo "Error: Parameters file not found."
    exit 1
fi

BINARY="build/lz77"
if [ ! -f "$BINARY" ]; then
    echo "Error: Binary '$BINARY' not found."
    exit 1
fi

TEMP_ENC="/tmp/encoded"
TEMP_DEC="/tmp/decoded"

# Remove CR from header
HEADER=$(head -n 1 "$PARAMS_FILE" | tr -d '\r')
echo "${HEADER},enc_time,dec_time,enc_size,ratio"

TOTAL_ROWS=$(tail -n +2 "$PARAMS_FILE" | wc -l)
CURRENT_ROW=0

# Process substitution prevents subshell. tr removes CRLF issues.
while IFS=, read -r input_file format block_size window_size future_limit prefix_size len3_dist_bits len4_dist_bits len5_dist_bits len6_dist_bits len7_dist_bits lenx_dist_bits lenx_len_bits; do

    ((CURRENT_ROW++))
    echo -ne "Progress: ${CURRENT_ROW}/${TOTAL_ROWS} ($((CURRENT_ROW * 100 / TOTAL_ROWS))%)\r" >&2

    if [ ! -f "$input_file" ]; then
        echo "Error: Input file '$input_file' not found. Skipping row." >&2
        continue
    fi

    ORIG_SIZE=$(wc -c <"$input_file")

    ENC_OUT=$($BINARY -e -i "$input_file" -o "$TEMP_ENC" -f "$format" -m \
        --block-size "$block_size" \
        --window-size "$window_size" \
        --future-limit "$future_limit" \
        --prefix-size "$prefix_size" \
        --len3-dist-bits "$len3_dist_bits" \
        --len4-dist-bits "$len4_dist_bits" \
        --len5-dist-bits "$len5_dist_bits" \
        --len6-dist-bits "$len6_dist_bits" \
        --len7-dist-bits "$len7_dist_bits" \
        --lenx-dist-bits "$lenx_dist_bits" \
        --lenx-len-bits "$lenx_len_bits" 2>&1)

    ENC_TIME=$(echo "$ENC_OUT" | grep "TIME:" | awk '{print $2}')

    DEC_OUT=$($BINARY -d -i "$TEMP_ENC" -o "$TEMP_DEC" -m \
        --block-size "$block_size" \
        --window-size "$window_size" \
        --future-limit "$future_limit" \
        --prefix-size "$prefix_size" \
        --len3-dist-bits "$len3_dist_bits" \
        --len4-dist-bits "$len4_dist_bits" \
        --len5-dist-bits "$len5_dist_bits" \
        --len6-dist-bits "$len6_dist_bits" \
        --len7-dist-bits "$len7_dist_bits" \
        --lenx-dist-bits "$lenx_dist_bits" \
        --lenx-len-bits "$lenx_len_bits" 2>&1)

    DEC_TIME=$(echo "$DEC_OUT" | grep "TIME:" | awk '{print $2}')
    ENC_SIZE=$(wc -c <"$TEMP_ENC")
    RATIO=$(awk "BEGIN {printf \"%.2f%%\", ($ENC_SIZE / $ORIG_SIZE) * 100}")

    if ! cmp -s "$input_file" "$TEMP_DEC"; then
        echo -e "\nError: Verification failed!" >&2
        echo "Failed parameters:" >&2
        echo "  file:            $input_file" >&2
        echo "  format:          $format" >&2
        echo "  block_size:      $block_size" >&2
        echo "  window_size:     $window_size" >&2
        echo "  future_limit:    $future_limit" >&2
        echo "  prefix_size:     $prefix_size" >&2
        echo "  len3_dist_bits:  $len3_dist_bits" >&2
        echo "  len4_dist_bits:  $len4_dist_bits" >&2
        echo "  len5_dist_bits:  $len5_dist_bits" >&2
        echo "  len6_dist_bits:  $len6_dist_bits" >&2
        echo "  len7_dist_bits:  $len7_dist_bits" >&2
        echo "  lenx_dist_bits:  $lenx_dist_bits" >&2
        echo "  lenx_len_bits:   $lenx_len_bits" >&2
        rm -f "$TEMP_ENC" "$TEMP_DEC"
        exit 1
    fi

    echo "${input_file},${format},${block_size},${window_size},${future_limit},${prefix_size},${len3_dist_bits},${len4_dist_bits},${len5_dist_bits},${len6_dist_bits},${len7_dist_bits},${lenx_dist_bits},${lenx_len_bits},${ENC_TIME},${DEC_TIME},${ENC_SIZE},${RATIO}"

done < <(tail -n +2 "$PARAMS_FILE" | tr -d '\r')

echo -e "\nFinished processing all benchmarks." >&2
rm -f "$TEMP_ENC" "$TEMP_DEC"

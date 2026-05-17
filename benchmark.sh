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

# Create output directory and file
mkdir -p benchmark-results
OUTPUT_FILE="benchmark-results/results.csv"

# Remove CR from header and replace time columns with speed columns (MB/s)
HEADER=$(head -n 1 "$PARAMS_FILE" | tr -d '\r')
echo "${HEADER},enc_speed_mbps,dec_speed_mbps,enc_size,ratio" > "$OUTPUT_FILE"
echo "Writing results to $OUTPUT_FILE" >&2

TOTAL_ROWS=$(tail -n +2 "$PARAMS_FILE" | wc -l)
CURRENT_ROW=0

# Process substitution prevents subshell. tr removes CRLF issues.
while IFS=, read -r input_file format block_size window_size future_limit max_matches len3_dist_bits len4_dist_bits len5_dist_bits len6_dist_bits len7_dist_bits lenx_dist_bits lenx_len_bits; do

    ((CURRENT_ROW++))
    echo -ne "Progress: ${CURRENT_ROW}/${TOTAL_ROWS} ($((CURRENT_ROW * 100 / TOTAL_ROWS))%)\r" >&2

    if [ ! -f "$input_file" ]; then
        echo "Error: Input file '$input_file' not found. Skipping row." >&2
        continue
    fi

    ORIG_SIZE=$(wc -c <"$input_file")

    # Run encoding 3 times and collect times
    ENC_TIMES=()
    for i in {1..3}; do
        ENC_OUT=$($BINARY -e -i "$input_file" -o "$TEMP_ENC" -f "$format" -m \
            --block-size "$block_size" \
            --window-size "$window_size" \
            --future-limit "$future_limit" \
            --max-matches "$max_matches" \
            --len3-dist-bits "$len3_dist_bits" \
            --len4-dist-bits "$len4_dist_bits" \
            --len5-dist-bits "$len5_dist_bits" \
            --len6-dist-bits "$len6_dist_bits" \
            --len7-dist-bits "$len7_dist_bits" \
            --lenx-dist-bits "$lenx_dist_bits" \
            --lenx-len-bits "$lenx_len_bits" 2>&1)
        TIME=$(echo "$ENC_OUT" | grep "TIME:" | awk '{print $2}')
        ENC_TIMES+=("$TIME")
    done

    # Run decoding 3 times and collect times
    DEC_TIMES=()
    for i in {1..3}; do
        DEC_OUT=$($BINARY -d -i "$TEMP_ENC" -o "$TEMP_DEC" -m \
            --block-size "$block_size" \
            --window-size "$window_size" \
            --future-limit "$future_limit" \
            --max-matches "$max_matches" \
            --len3-dist-bits "$len3_dist_bits" \
            --len4-dist-bits "$len4_dist_bits" \
            --len5-dist-bits "$len5_dist_bits" \
            --len6-dist-bits "$len6_dist_bits" \
            --len7-dist-bits "$len7_dist_bits" \
            --lenx-dist-bits "$lenx_dist_bits" \
            --lenx-len-bits "$lenx_len_bits" 2>&1)
        TIME=$(echo "$DEC_OUT" | grep "TIME:" | awk '{print $2}')
        DEC_TIMES+=("$TIME")
    done

    # Compute average times
    ENC_TIME=$(awk -v t1="${ENC_TIMES[0]}" -v t2="${ENC_TIMES[1]}" -v t3="${ENC_TIMES[2]}" 'BEGIN {print (t1 + t2 + t3) / 3}')
    DEC_TIME=$(awk -v t1="${DEC_TIMES[0]}" -v t2="${DEC_TIMES[1]}" -v t3="${DEC_TIMES[2]}" 'BEGIN {print (t1 + t2 + t3) / 3}')
    ENC_SIZE=$(wc -c <"$TEMP_ENC")

    # Calculate Megabytes per second (MB/s) and compression ratio
    SPEED_STATS=$(awk -v orig="$ORIG_SIZE" -v enc_sz="$ENC_SIZE" -v t_enc="$ENC_TIME" -v t_dec="$DEC_TIME" 'BEGIN {
        enc_mbps = (t_enc > 0) ? (orig / 1048576) / t_enc : 0;
        dec_mbps = (t_dec > 0) ? (orig / 1048576) / t_dec : 0;
        ratio = (orig > 0) ? (enc_sz / orig) * 100 : 0;
        printf "%.2f,%.2f,%.2f%%", enc_mbps, dec_mbps, ratio
    }')

    IFS=, read -r ENC_SPEED DEC_SPEED RATIO <<<"$SPEED_STATS"

    if ! cmp -s "$input_file" "$TEMP_DEC"; then
        echo -e "\nError: Verification failed!" >&2
        echo "Failed parameters:" >&2
        echo "  file:            $input_file" >&2
        echo "  format:          $format" >&2
        echo "  block_size:      $block_size" >&2
        echo "  window_size:     $window_size" >&2
        echo "  future_limit:    $future_limit" >&2
        echo "  max_matches:     $max_matches" >&2
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

    echo "${input_file},${format},${block_size},${window_size},${future_limit},${max_matches},${len3_dist_bits},${len4_dist_bits},${len5_dist_bits},${len6_dist_bits},${len7_dist_bits},${lenx_dist_bits},${lenx_len_bits},${ENC_SPEED},${DEC_SPEED},${ENC_SIZE},${RATIO}" >> "$OUTPUT_FILE"

done < <(tail -n +2 "$PARAMS_FILE" | tr -d '\r')

echo -e "\nFinished processing all benchmarks." >&2
echo "Results saved to: $OUTPUT_FILE" >&2
rm -f "$TEMP_ENC" "$TEMP_DEC"

#!/bin/bash

compressor=$1
to_encode=$2

# Define the CPU core to pin for decompression (default is core 0)
DECODE_CORE=0

# Shift positional parameters by 2, leaving only the trailing arguments in "$@"
shift 2

phsize() {
    ls -lh "$1" | awk '{print $5}'
}

psize() {
    ls -l "$1" | awk '{print $5}'
}

# Create unique temporary files for encoding and decoding
encoded_tmp=$(mktemp)
decoded_tmp=$(mktemp)

# Ensure temporary files are removed on exit (success or failure)
cleanup() {
    rm -f "$encoded_tmp" "$decoded_tmp"
}
trap cleanup EXIT

echo "Encoding..."
exec 3>&1
# Capture peak RSS (%M), User CPU (%U), and System CPU (%S)
enc_err=$({ /usr/bin/time -f "BENCHMARK: %M %U %S" "$compressor" -e -i "$to_encode" -o "$encoded_tmp" -t "$@"; } 2>&1 1>&3)
enc_status=$?
exec 3>&-

if [ $enc_status -ne 0 ]; then
    echo "Command failed with exit code $enc_status"
    if [ $enc_status -eq 139 ]; then
        echo "Error: Segmentation fault (Signal 11)"
    elif [ $enc_status -gt 128 ]; then
        echo "Error: Terminated by signal $((enc_status - 128))"
    fi
    echo "--- Stderr Output ---"
    echo "$enc_err" | grep -v "BENCHMARK:"
    echo "---------------------"
    echo "To debug this failure, run:"
    echo "gdb --args $compressor -e -i $to_encode -o $encoded_tmp -t $@"
    exit 1
fi

enc_kb=$(echo "$enc_err" | awk '/BENCHMARK:/ {print $2}')
enc_user=$(echo "$enc_err" | awk '/BENCHMARK:/ {print $3}')
enc_sys=$(echo "$enc_err" | awk '/BENCHMARK:/ {print $4}')
enc_sec=$(awk -v u="$enc_user" -v s="$enc_sys" 'BEGIN {printf "%.2f", u + s}')

echo "Decoding (Pinned to core ${DECODE_CORE}, forced single-thread)..."
exec 4>&1

# Suppress raw output while collecting structural metrics
dec_err=$({ env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 taskset -c "${DECODE_CORE}" perf stat -x, -e duration_time /usr/bin/time -f "BENCHMARK: %M" "$compressor" -d -i "$encoded_tmp" -o "$decoded_tmp"; } 2>&1 1>&4)
dec_status=$?
exec 4>&-

if [ $dec_status -ne 0 ]; then
    echo "Command failed with exit code $dec_status"
    if [ $dec_status -eq 139 ]; then
        echo "Error: Segmentation fault (Signal 11)"
    elif [ $dec_status -gt 128 ]; then
        echo "Error: Terminated by signal $((dec_status - 128))"
    fi
    echo "--- Stderr Output ---"
    echo "$dec_err" | grep -v "BENCHMARK:"
    echo "---------------------"
    echo "To debug this failure, run:"
    echo "$compressor -e -i $to_encode -o $encoded_tmp -t $@ && taskset -c ${DECODE_CORE} gdb --args $compressor -d -i $encoded_tmp -o $decoded_tmp"
    exit 1
fi

# Parse memory usage from the inner time command
dec_kb=$(echo "$dec_err" | awk '/BENCHMARK:/ {print $2}')

# Parse duration_time out of the CSV-formatted perf output
dec_ns=$(echo "$dec_err" | awk -F, '/duration_time/ {print $1}')
dec_sec=$(awk -v ns="$dec_ns" 'BEGIN {printf "%.4f", ns / 1000000000}')

# Calculate compression ratio in %
orig_size=$(psize "$to_encode")
comp_size=$(psize "$encoded_tmp")
ratio=$(awk -v c="$comp_size" -v o="$orig_size" 'BEGIN {printf "%.2f", (c/o)*100}')

# Convert peak memory KB to MB
enc_mb=$(awk -v k="$enc_kb" 'BEGIN {printf "%.2f", k/1024}')
dec_mb=$(awk -v k="$dec_kb" 'BEGIN {printf "%.2f", k/1024}')

echo "    Compression: $(phsize "$encoded_tmp") ($(psize "$encoded_tmp") bytes) / $(phsize "$to_encode") [Ratio: ${ratio}%]"
echo "    CPU Time (Enc): ${enc_sec} s"
echo "    Precise Time (Dec): ${dec_sec} s"
echo "    Max RAM Usage (Enc): ${enc_mb} MB"
echo "    Max RAM Usage (Dec): ${dec_mb} MB"

cmp "$to_encode" "$decoded_tmp"
res=$?
if [ "$res" == "0" ]; then
    echo "OK!"
    exit 0
else
    echo "FAIL"
    exit 1
fi


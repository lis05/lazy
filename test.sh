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

# Poll the process tree rooted at $1 every 0.1s, sampling VmPeak each tick.
# VmPeak is monotonically increasing so we just keep the last non-zero reading
# and write it to $2 when the process exits.
poll_vmpeak() {
    local pid=$1 outfile=$2 last=0
    while kill -0 "$pid" 2>/dev/null; do
        local sample
        sample=$(awk -v root="$pid" '
            /^Pid:/   { cur = $2 }
            /^PPid:/  { ppid[cur] = $2 }
            /^VmPeak:/{ vmpeak[cur] = $2 }
            END {
                for (c in ppid) children[ppid[c]] = children[ppid[c]] " " c
                n = 1; queue[0] = root; total = 0
                while (n > 0) {
                    p = queue[--n]
                    total += vmpeak[p]+0
                    split(children[p], ch, " ")
                    for (i in ch) if (ch[i]+0 > 0) queue[n++] = ch[i]
                }
                print total
            }
        ' /proc/*/status 2>/dev/null)
        [[ "${sample:-0}" -gt 0 ]] && last=$sample
        sleep 0.1
    done
    echo "$last" > "$outfile"
}

# Create unique temporary files for encoding and decoding
encoded_tmp=$(mktemp)
decoded_tmp=$(mktemp)
enc_mem_file=$(mktemp)
dec_mem_file=$(mktemp)

# Ensure temporary files are removed on exit (success or failure)
cleanup() {
    rm -f "$encoded_tmp" "$decoded_tmp" "$enc_mem_file" "$dec_mem_file"
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Encoding
# ---------------------------------------------------------------------------
echo "Encoding..."

enc_start=$(date +%s%N)

"$compressor" -e -i "$to_encode" -o "$encoded_tmp" "$@" &
enc_pid=$!

poll_vmpeak "$enc_pid" "$enc_mem_file" &
poll_pid=$!

wait "$enc_pid"
enc_status=$?
wait "$poll_pid"

enc_end=$(date +%s%N)

if [ $enc_status -ne 0 ]; then
    echo "Command failed with exit code $enc_status"
    if [ $enc_status -eq 139 ]; then
        echo "Error: Segmentation fault (Signal 11)"
    elif [ $enc_status -gt 128 ]; then
        echo "Error: Terminated by signal $((enc_status - 128))"
    fi
    echo "To debug this failure, run:"
    echo "gdb --args $compressor -e -i $to_encode -o $encoded_tmp $*"
    exit 1
fi

enc_kb=$(cat "$enc_mem_file")
enc_kb=${enc_kb:-0}
enc_sec=$(awk -v s="$enc_start" -v e="$enc_end" 'BEGIN {printf "%.2f", (e - s) / 1000000000}')

# ---------------------------------------------------------------------------
# Decoding  (pinned to a single core)
# ---------------------------------------------------------------------------
echo "Decoding (Pinned to core ${DECODE_CORE}, forced single-thread)..."

dec_start=$(date +%s%N)

env OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 \
    taskset -c "${DECODE_CORE}" \
    "$compressor" -d -i "$encoded_tmp" -o "$decoded_tmp" "$@" &
dec_pid=$!

poll_vmpeak "$dec_pid" "$dec_mem_file" &
poll_pid=$!

wait "$dec_pid"
dec_status=$?
wait "$poll_pid"

dec_end=$(date +%s%N)

if [ $dec_status -ne 0 ]; then
    echo "Command failed with exit code $dec_status"
    if [ $dec_status -eq 139 ]; then
        echo "Error: Segmentation fault (Signal 11)"
    elif [ $dec_status -gt 128 ]; then
        echo "Error: Terminated by signal $((dec_status - 128))"
    fi
    echo "To debug this failure, run:"
    echo "$compressor -e -i $to_encode -o $encoded_tmp $* && taskset -c ${DECODE_CORE} gdb --args $compressor -d -i $encoded_tmp -o $decoded_tmp $*"
    exit 1
fi

dec_kb=$(cat "$dec_mem_file")
dec_kb=${dec_kb:-0}
dec_sec=$(awk -v s="$dec_start" -v e="$dec_end" 'BEGIN {printf "%.4f", (e - s) / 1000000000}')

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
orig_size=$(psize "$to_encode")
comp_size=$(psize "$encoded_tmp")
ratio=$(awk -v c="$comp_size" -v o="$orig_size" 'BEGIN {printf "%.2f", (c/o)*100}')

enc_mb=$(awk -v k="$enc_kb" 'BEGIN {printf "%.2f", k/1024}')
dec_mb=$(awk -v k="$dec_kb" 'BEGIN {printf "%.2f", k/1024}')

echo "    Compression: $(phsize "$encoded_tmp") ($(psize "$encoded_tmp") bytes) / $(phsize "$to_encode") [Ratio: ${ratio}%]"
echo "    Compression Time   : ${enc_sec} s"
echo "    Decompression Time : ${dec_sec} s"
echo "    Max VmPeak (Enc)   : ${enc_mb} MB"
echo "    Max VmPeak (Dec)   : ${dec_mb} MB"

cmp "$to_encode" "$decoded_tmp"
res=$?
if [ "$res" == "0" ]; then
    echo "OK!"
    exit 0
else
    echo "FAIL"
    exit 1
fi

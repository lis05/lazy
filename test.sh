compressor=$1
to_encode=$2

# Shift positional parameters by 2, leaving only the trailing arguments in "$@"
shift 2

phsize() {
    ls -lh "$1" | awk '{print $5}'
}

psize() {
    ls -l "$1" | awk '{print $5}'
}

rm /tmp/encoded /tmp/decoded 1>/dev/null 2>&1

echo "Encoding..."
exec 3>&1
# Capture stderr directly into a variable without breaking pipelines
enc_err=$(/usr/bin/time -f "Memory: %M KB" $compressor -e -i $to_encode -o /tmp/encoded -t "$@" 2>&1 1>&3)
enc_status=$?
exec 3>&-

if [ $enc_status -ne 0 ]; then
    echo "Command failed with exit code $enc_status"
    echo "$enc_err" | grep -v "Memory:"
    exit 1
fi

enc_kb=$(echo "$enc_err" | awk '/Memory:/ {print $2}')

echo "Decoding..."
exec 3>&1
dec_err=$(/usr/bin/time -f "Memory: %M KB" $compressor -d -i /tmp/encoded -o /tmp/decoded "$@" 2>&1 1>&3)
dec_status=$?
exec 3>&-

if [ $dec_status -ne 0 ]; then
    echo "Command failed with exit code $dec_status"
    echo "$dec_err" | grep -v "Memory:"
    exit 1
fi

dec_kb=$(echo "$dec_err" | awk '/Memory:/ {print $2}')

# Calculate compression ratio in %
orig_size=$(psize "$to_encode")
comp_size=$(psize /tmp/encoded)
ratio=$(awk -v c="$comp_size" -v o="$orig_size" 'BEGIN {printf "%.2f", (c/o)*100}')

# Convert peak memory KB to MB
enc_mb=$(awk -v k="$enc_kb" 'BEGIN {printf "%.2f", k/1024}')
dec_mb=$(awk -v k="$dec_kb" 'BEGIN {printf "%.2f", k/1024}')

echo "    Compression: $(phsize /tmp/encoded) ($(psize /tmp/encoded) bytes) / $(phsize $to_encode) [Ratio: ${ratio}%]"
echo "    Max RAM Usage (Enc): ${enc_mb} MB"
echo "    Max RAM Usage (Dec): ${dec_mb} MB"

cmp $to_encode /tmp/decoded
res=$?
if [ "$res" == "0" ]; then
    echo "OK!"
    exit 0
else
    echo "FAIL"
    exit 1
fi


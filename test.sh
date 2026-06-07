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

TIMEFORMAT="Encoding: %R"
time $compressor -e -i $to_encode -o /tmp/encoded -t "$@" || \
    echo "Failed command: $compressor -e -i $to_encode -o /tmp/encoded -t $@"

TIMEFORMAT="Decoding: %R"
time $compressor -d -i /tmp/encoded -o /tmp/decoded "$@" || \
    echo "Failed command: $compressor -d -i /tmp/encoded -o /tmp/decoded $@"

# Calculate compression ratio in %
orig_size=$(psize "$to_encode")
comp_size=$(psize /tmp/encoded)
ratio=$(awk -v c="$comp_size" -v o="$orig_size" 'BEGIN {printf "%.2f", (c/o)*100}')

echo "    Compression: $(phsize /tmp/encoded) ($(psize /tmp/encoded) bytes) / $(phsize $to_encode) [Ratio: ${ratio}%]"

cmp $to_encode /tmp/decoded
res=$?
if [ "$res" == "0" ]; then
    echo "OK!"
    exit 0
else
    echo "FAIL"
    exit 1
fi

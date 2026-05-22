compressor=$1
to_encode=$2
format=$3

# Shift positional parameters by 3, leaving only the trailing arguments in "$@"
shift 3

phsize() {
    ls -lh "$1" | awk '{print $5}'
}

psize() {
    ls -l "$1" | awk '{print $5}'
}

rm /tmp/encoded /tmp/decoded 1>/dev/null 2>&1

TIMEFORMAT="Encoding: %R"
time $compressor -e -i $to_encode -o /tmp/encoded -f $format -t "$@" || \
    echo "Failed command: $compressor -e -i $to_encode -o /tmp/encoded -f $format -t $@"

TIMEFORMAT="Decoding: %R"
time $compressor -d -i /tmp/encoded -o /tmp/decoded "$@" || \
    echo "Failed command: $compressor -d -i /tmp/encoded -o /tmp/decoded $@"

echo "   Compression: $(phsize /tmp/encoded) ($(psize /tmp/encoded)) / $(phsize $to_encode)"

cmp $to_encode /tmp/decoded
res=$?
if [ "$res" == "0" ]; then
    echo "OK!"
    exit 0
else
    echo "FAIL"
    echo "============= $to_encode ============="
    head -20 $to_encode
    echo "============= encoded ============="
    head -20 /tmp/encoded
    echo "============= decoded ============="
    head -20 /tmp/decoded
    exit 1
fi

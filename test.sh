compressor=$1
to_encode=$2
format=$3

psize() {
     ls -lh "$1" | awk '{print $5}'
}

TIMEFORMAT="Encoding: %R"
time $compressor -e -i $to_encode -o /tmp/encoded -f $format -t

TIMEFORMAT="Decoding: %R"
time $compressor -d -i /tmp/encoded -o /tmp/decoded

echo "    Compression: $(psize /tmp/encoded) / $(psize $to_encode)"

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

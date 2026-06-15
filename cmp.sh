#!/usr/bin/env bash

set -u

if [ -z "${1:-}" ]; then
    echo "Usage: $0 <directory>"
    exit 1
fi

TARGET_DIR="$1"
LZMPO_BIN="./build/lzmpo"

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "compressor | file | ratio in % | compression time | decompression time | compression memory in MB | decompression memory in MB"

run_cmd() {
    local TIME_FILE="$1"
    shift

    /usr/bin/time \
        -o "$TIME_FILE" \
        -f "%e %M" \
        "$@"
}

run_test() {
    local NAME="$1"
    local FILE="$2"
    local BASENAME="$3"
    local ORIG_SIZE="$4"

    shift 4

    local ENC=("$@")

    local COMP_FILE="${TMP_DIR}/${BASENAME}.${NAME}"
    local DECOMP_FILE="${COMP_FILE}.dec"

    local ENC_TIME_FILE="${TMP_DIR}/enc.time"
    local DEC_TIME_FILE="${TMP_DIR}/dec.time"

    rm -f \
        "$COMP_FILE" \
        "$DECOMP_FILE" \
        "$ENC_TIME_FILE" \
        "$DEC_TIME_FILE"

    case "$NAME" in

        brotli_*)
            run_cmd "$ENC_TIME_FILE" \
                "${ENC[@]}" \
                -c "$FILE" >"$COMP_FILE"

            run_cmd "$DEC_TIME_FILE" \
                brotli -d -c "$COMP_FILE" >"$DECOMP_FILE"
            ;;

        xz_*)
            run_cmd "$ENC_TIME_FILE" \
                "${ENC[@]}" \
                -c "$FILE" >"$COMP_FILE"

            run_cmd "$DEC_TIME_FILE" \
                xz -d -c "$COMP_FILE" >"$DECOMP_FILE"
            ;;

        zstd_*)
            run_cmd "$ENC_TIME_FILE" \
                "${ENC[@]}" \
                -c "$FILE" >"$COMP_FILE"

            run_cmd "$DEC_TIME_FILE" \
                zstd -d -q -c "$COMP_FILE" >"$DECOMP_FILE"
            ;;

        lzmpo_*)
            run_cmd "$ENC_TIME_FILE" \
                "${ENC[@]}" \
                -e \
                -i "$FILE" \
                -o "$COMP_FILE" \
                -p \
                >&2

            run_cmd "$DEC_TIME_FILE" \
                "$LZMPO_BIN" \
                -d \
                -i "$COMP_FILE" \
                -o "$DECOMP_FILE" \
                -p \
                >&2
            ;;
    esac

    ENC_TIME=0.00
    ENC_MEM=0
    DEC_TIME=0.00
    DEC_MEM=0

    [ -f "$ENC_TIME_FILE" ] && read -r ENC_TIME ENC_MEM <"$ENC_TIME_FILE"
    [ -f "$DEC_TIME_FILE" ] && read -r DEC_TIME DEC_MEM <"$DEC_TIME_FILE"

    [[ "$ENC_MEM" =~ ^[0-9]+$ ]] || ENC_MEM=0
    [[ "$DEC_MEM" =~ ^[0-9]+$ ]] || DEC_MEM=0

    COMP_SIZE=$(stat -c %s "$COMP_FILE" 2>/dev/null || echo 0)

    if [ "$COMP_SIZE" -gt 0 ]; then
        RATIO=$(awk "BEGIN {printf \"%.6f\", ($COMP_SIZE/$ORIG_SIZE)*100}")
    else
        RATIO="ERR"
    fi

    ENC_MB=$(awk "BEGIN {printf \"%.2f\", $ENC_MEM/1024}")
    DEC_MB=$(awk "BEGIN {printf \"%.2f\", $DEC_MEM/1024}")

    echo "$NAME | $BASENAME | $RATIO | $ENC_TIME | $DEC_TIME | $ENC_MB | $DEC_MB"

    rm -f \
        "$COMP_FILE" \
        "$DECOMP_FILE" \
        "$ENC_TIME_FILE" \
        "$DEC_TIME_FILE"
}

find "$TARGET_DIR" \
    -type f \
    ! -name "*.brotli" \
    ! -name "*.xz" \
    ! -name "*.zstd" \
    ! -name "*.lzmpo" \
    ! -name "*.dec" \
    | while read -r FILE; do

        ORIG_SIZE=$(stat -c %s "$FILE")

        [ "$ORIG_SIZE" -eq 0 ] && continue

        BASENAME=$(basename "$FILE")

        for l in {0..11}; do
            run_test \
                "brotli_q$l" \
                "$FILE" \
                "$BASENAME" \
                "$ORIG_SIZE" \
                brotli -q "$l"
        done

        for l in {0..9}; do
            run_test \
                "xz_$l" \
                "$FILE" \
                "$BASENAME" \
                "$ORIG_SIZE" \
                xz "-$l" -T16
        done

        for l in {1..22}; do
            if [ "$l" -ge 20 ]; then
                run_test \
                    "zstd_$l" \
                    "$FILE" \
                    "$BASENAME" \
                    "$ORIG_SIZE" \
                    zstd --ultra "-$l" -T16 -q
            else
                run_test \
                    "zstd_$l" \
                    "$FILE" \
                    "$BASENAME" \
                    "$ORIG_SIZE" \
                    zstd "-$l" -T16 -q
            fi
        done

        for l in {0..8}; do
            if [[ "$BASENAME" == "enwik9" && "$l" -eq 8 ]]; then
                continue
            fi

            run_test \
                "lzmpo_$l" \
                "$FILE" \
                "$BASENAME" \
                "$ORIG_SIZE" \
                "$LZMPO_BIN" "-$l"
        done

    done

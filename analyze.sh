#!/bin/bash

# Configuration
RESULTS_FILE="benchmark-results/results.csv"

if [ ! -f "$RESULTS_FILE" ]; then
    echo "Error: Result file '$RESULTS_FILE' not found." >&2
    exit 1
fi

echo "========================================================="
echo "               BENCHMARK ANALYSIS RESULTS                "
echo "========================================================="

# Helper function processes all sort outputs completely to prevent broken pipes
print_table() {
    awk -F, '
    BEGIN {
        printf "%-7s | %-10s | %-11s | %-12s | %-9s | %-9s | %-6s\n", "Format", "Block Size", "Window Size", "Future Limit", "Enc MB/s", "Dec MB/s", "Ratio"
        print "-------------------------------------------------------------------------------"
    }
    {
        printf "%-7s | %-10s | %-11s | %-12s | %-9s | %-9s | %-6s\n", $2, $3, $4, $5, $14, $15, $17
    }'
}

echo -e "\n[ 5 BEST COMPRESSORS BY RATIO ]"
tail -n +2 "$RESULTS_FILE" | sort -t, -k17,17n | awk 'NR<=5' | print_table

echo -e "\n[ 5 BEST COMPRESSORS BY ENCODING SPEED ]"
tail -n +2 "$RESULTS_FILE" | sort -t, -k14,14nr | awk 'NR<=5' | print_table

echo -e "\n[ 5 BEST COMPRESSORS BY DECODING SPEED ]"
tail -n +2 "$RESULTS_FILE" | sort -t, -k15,15nr | awk 'NR<=5' | print_table

echo -e "\n[ 5 BEST COMPRESSORS OVERALL (BALANCED SCORE) ]"
tail -n +2 "$RESULTS_FILE" | awk -F, '
{
    split($17, r, "%")
    ratio = r[1]
    enc = $14
    dec = $15
    
    if (enc <= 0) enc = 0.1
    if (dec <= 0) dec = 0.1
    
    score = ratio / (log(enc) + log(dec))
    print score "," $0
}' | sort -t, -k1,1n | awk 'NR<=5' | cut -d, -f2- | print_table

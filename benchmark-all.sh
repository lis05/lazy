#!/bin/bash

PARAMS_DIR="benchmark-params"
RESULTS_DIR="benchmark-results"
BENCHMARK_SCRIPT="./benchmark.sh"
MASTER_RESULT="$RESULTS_DIR/results.csv"

if [ ! -d "$PARAMS_DIR" ]; then
    echo "Error: Parameters directory '$PARAMS_DIR' does not exist." >&2
    exit 1
fi

if [ ! -x "$BENCHMARK_SCRIPT" ]; then
    echo "Error: Benchmark script '$BENCHMARK_SCRIPT' not found or not executable." >&2
    exit 1
fi

mkdir -p "$RESULTS_DIR"

# Step 1: Clean old master results file
rm -f "$MASTER_RESULT"

# Step 2: Run all Python scripts to generate CSV parameter files
for PY_FILE in "$PARAMS_DIR"/*.py; do
    [ -e "$PY_FILE" ] || continue
    echo "Executing generator: $(basename "$PY_FILE")..." >&2

    (cd "$PARAMS_DIR" && python3 "$(basename "$PY_FILE")")
done

IS_FIRST_FILE=true

# Step 3: Run benchmarks and append to the single master file
for PARAMS_FILE in "$PARAMS_DIR"/*.csv; do
    [ -e "$PARAMS_FILE" ] || continue

    FILENAME=$(basename "$PARAMS_FILE")
    echo "Running benchmark for $FILENAME..." >&2

    if [ "$IS_FIRST_FILE" = true ]; then
        # Keep header for the first file
        "$BENCHMARK_SCRIPT" "$PARAMS_FILE" >>"$MASTER_RESULT"
        IS_FIRST_FILE=false
    else
        # Strip header for subsequent files
        "$BENCHMARK_SCRIPT" "$PARAMS_FILE" | tail -n +2 >>"$MASTER_RESULT"
    fi

    echo "Finished $FILENAME -> $MASTER_RESULT" >&2
done

rm "$PARAMS_DIR"/*.csv 1>/dev/null 2>&1

echo "All benchmarks completed. Output saved to $MASTER_RESULT" >&2

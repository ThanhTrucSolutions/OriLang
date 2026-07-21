#!/bin/bash
# Golden output tests for OriLang beginner samples.
# Requires the OriLang VM built and available as `ori` in PATH.
# Usage: bash tests/golden_test.sh

set -e

SAMPLES=("beginner_01_hello" "beginner_23_fizzbuzz" "beginner_24_factorial")
RESULTS_DIR="tests/golden_results"
PASS=0
FAIL=0

mkdir -p "$RESULTS_DIR"

for sample in "${SAMPLES[@]}"; do
    echo "=== Testing $sample ==="
    out="$RESULTS_DIR/$sample.out"
    expected="tests/expected/$sample.txt"

    ori run "samples/$sample.ori" > "$out" 2>&1

    if [ -f "$expected" ]; then
        if diff -q "$out" "$expected" > /dev/null 2>&1; then
            echo "  PASS (matches expected)"
            ((PASS++))
        else
            echo "  FAIL (differs from expected)"
            diff "$out" "$expected" || true
            ((FAIL++))
        fi
    else
        echo "  INFO (no expected output; saved to $out)"
        ((PASS++))
    fi
done

echo ""
echo "Results: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]

#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

echo "=== Pipeline Integration Tests ==="

# 1. Two-stage pipeline
OUT=$($SHELLCRAFT -c "printf 'apple\nbanana\ncherry\n' | grep banana")
if [ "$OUT" != "banana" ]; then
    echo "[FAIL] Two-stage pipeline: expected 'banana', got '$OUT'"
    exit 1
fi
echo "[PASS] Two-stage pipeline"

# 2. Three-stage pipeline
OUT=$($SHELLCRAFT -c "printf '3\n1\n2\n' | sort -n | head -n 1")
if [ "$OUT" != "1" ]; then
    echo "[FAIL] Three-stage pipeline: expected '1', got '$OUT'"
    exit 1
fi
echo "[PASS] Three-stage pipeline"

# 3. Four-stage pipeline
OUT=$($SHELLCRAFT -c "printf 'line1\nline2\nline3\nline4\n' | grep line | tail -n 2 | wc -l")
OUT=$(echo "$OUT" | tr -d ' ')
if [ "$OUT" != "2" ]; then
    echo "[FAIL] Four-stage pipeline: expected '2', got '$OUT'"
    exit 1
fi
echo "[PASS] Four-stage pipeline"

# 4. Pipeline exit code propagation (last command status)
set +e
$SHELLCRAFT -c "printf 'hello\n' | grep nonexistent"
STATUS=$?
set -e
if [ "$STATUS" -eq 0 ]; then
    echo "[FAIL] Pipeline status propagation: expected non-zero, got $STATUS"
    exit 1
fi
echo "[PASS] Pipeline exit status propagation"

echo "All pipeline tests passed!"

#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

echo "=== Command Substitution Integration Tests ==="

# 1. Simple command substitution
OUT=$($SHELLCRAFT -c "echo Current user is \$(whoami)")
EXPECTED="Current user is $(whoami)"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "[FAIL] Simple substitution: expected '$EXPECTED', got '$OUT'"
    exit 1
fi
echo "[PASS] Simple command substitution"

# 2. Pipeline command substitution
OUT=$($SHELLCRAFT -c "echo Count: \$(printf '1\n2\n3\n' | wc -l | tr -d ' ')")
if [ "$OUT" != "Count: 3" ]; then
    echo "[FAIL] Pipeline substitution: expected 'Count: 3', got '$OUT'"
    exit 1
fi
echo "[PASS] Pipeline command substitution"

# 3. Arithmetic / date substitution
OUT=$($SHELLCRAFT -c "echo Date: \$(date +%Y)")
CURRENT_YEAR=$(date +%Y)
if [ "$OUT" != "Date: $CURRENT_YEAR" ]; then
    echo "[FAIL] Date substitution: expected 'Date: $CURRENT_YEAR', got '$OUT'"
    exit 1
fi
echo "[PASS] Date command substitution"

echo "All command substitution tests passed!"

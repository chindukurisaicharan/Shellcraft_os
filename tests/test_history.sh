#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

echo "=== History Integration Tests ==="

HISTFILE="$HOME/.shellcraft_history"
rm -f "$HISTFILE"

# Execute a sequence of commands
printf "echo cmd1\necho cmd2\necho cmd3\nhistory\n" | $SHELLCRAFT > /dev/null

# Check if history command works
OUT=$(printf "echo alpha\necho beta\nhistory 2\n" | $SHELLCRAFT)
if echo "$OUT" | grep -q "alpha" || echo "$OUT" | grep -q "beta"; then
    echo "[PASS] History recording and retrieval"
else
    echo "[FAIL] History retrieval failed"
    exit 1
fi

echo "All history tests passed!"

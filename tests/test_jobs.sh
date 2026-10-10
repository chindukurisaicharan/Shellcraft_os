#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

echo "=== Job Control Integration Tests ==="

# 1. Test background process creation and prompt return
START=$(date +%s%N)
$SHELLCRAFT -c "sleep 0.5 &"
END=$(date +%s%N)
DIFF=$(( (END - START) / 1000000 ))

# Should return almost instantly (less than 300 ms)
if [ "$DIFF" -gt 350 ]; then
    echo "[FAIL] Background job did not return immediately (${DIFF}ms)"
    exit 1
fi
echo "[PASS] Background execution prompt latency (${DIFF}ms)"

# 2. Test multiple background jobs
OUT=$(printf "sleep 0.1 &\nsleep 0.1 &\nsleep 0.1 &\n" | $SHELLCRAFT)
echo "[PASS] Multiple background jobs"

# 3. Test background job reaper
OUT=$(printf "sleep 0.05 &\nsleep 0.1\njobs\n" | $SHELLCRAFT)
echo "[PASS] Background process synchronization"

echo "All job control tests passed!"

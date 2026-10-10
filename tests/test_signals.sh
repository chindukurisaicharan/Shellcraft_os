#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

echo "=== Signal Handling Integration Tests ==="

# 1. Shell remains alive when receiving SIGINT during interactive session simulation
sleep 2 | $SHELLCRAFT &
PID=$!
sleep 0.1

# Send SIGINT to shell process
kill -INT $PID 2>/dev/null || true
sleep 0.1

# Check if process is still alive
if kill -0 $PID 2>/dev/null; then
    echo "[PASS] Shellcraft ignores SIGINT (does not crash)"
    kill -TERM $PID 2>/dev/null || true
    wait $PID 2>/dev/null || true
else
    echo "[FAIL] Shellcraft terminated upon SIGINT"
    exit 1
fi

# 2. Command signal termination exit code propagation (128 + SIGKILL = 137 or 128 + SIGTERM = 143)
set +e
$SHELLCRAFT -c "sh -c 'kill -TERM \$\$'"
STATUS=$?
set -e
if [ "$STATUS" -ge 128 ]; then
    echo "[PASS] Signal exit code propagation (status=$STATUS)"
else
    echo "[FAIL] Expected status >= 128, got $STATUS"
    exit 1
fi

echo "All signal handling tests passed!"

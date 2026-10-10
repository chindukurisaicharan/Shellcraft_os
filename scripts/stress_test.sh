#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

if [ ! -x "$SHELLCRAFT" ]; then
    make
fi

echo "=== Shellcraft 500+ Commands Stress Test ==="

NUM_COMMANDS=500
TMP_INPUT="/tmp/shellcraft_stress_commands.txt"
rm -f "$TMP_INPUT"

echo "# Generating $NUM_COMMANDS stress test commands..."
for i in $(seq 1 $NUM_COMMANDS); do
    MOD=$(( i % 5 ))
    case $MOD in
        0) printf "echo 'Stress Iteration %d' > /dev/null\n" "$i" >> "$TMP_INPUT" ;;
        1) printf "true && false || echo 'fallback %d' > /dev/null\n" "$i" >> "$TMP_INPUT" ;;
        2) printf "echo 'a b c' | grep b | wc -l > /dev/null\n" >> "$TMP_INPUT" ;;
        3) printf "export STRESS_VAR_%d=%d\n" "$i" "$i" >> "$TMP_INPUT" ;;
        4) printf "echo \$(echo %d) > /dev/null\n" "$i" >> "$TMP_INPUT" ;;
    esac
done

START_TIME=$(date +%s%N)
cat "$TMP_INPUT" | $SHELLCRAFT > /dev/null
END_TIME=$(date +%s%N)

DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
rm -f "$TMP_INPUT"

echo "[PASS] Successfully executed $NUM_COMMANDS commands in ${DURATION_MS} ms!"
echo "Average latency per command: $(( DURATION_MS / NUM_COMMANDS )) ms"

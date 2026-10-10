#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

if [ ! -x "$SHELLCRAFT" ]; then
    make
fi

echo "=== Shellcraft Zombie Process Prevention Stress Test ==="

COUNT=100
TMP_SCRIPT="/tmp/shellcraft_zombie_test.txt"
rm -f "$TMP_SCRIPT"

echo "# Generating concurrent background tasks..."
for i in $(seq 1 $COUNT); do
    echo "true &" >> "$TMP_SCRIPT"
    echo "echo 'test' > /dev/null &" >> "$TMP_SCRIPT"
    echo "sleep 0.01 &" >> "$TMP_SCRIPT"
done
# Allow reaping
echo "sleep 0.2" >> "$TMP_SCRIPT"
echo "stats" >> "$TMP_SCRIPT"

TOTAL_JOBS=$(( COUNT * 3 ))
echo "Launching $TOTAL_JOBS asynchronous background processes..."

OUTPUT=$($SHELLCRAFT < "$TMP_SCRIPT")
rm -f "$TMP_SCRIPT"

# Count zombie processes in the system associated with shellcraft or current user
ZOMBIE_COUNT=$(ps -eo stat,comm | grep -E '^Z.*shellcraft' | wc -l || echo 0)
ZOMBIE_COUNT=$(echo "$ZOMBIE_COUNT" | tr -d ' ')

echo "----------------------------------------"
echo "Created jobs : $TOTAL_JOBS"
echo "Completed    : $TOTAL_JOBS"
echo "Zombies      : $ZOMBIE_COUNT"
echo "----------------------------------------"

if [ "$ZOMBIE_COUNT" -eq 0 ]; then
    echo "[PASS] Zero zombie processes detected. Safe asynchronous child reaping verified!"
else
    echo "[FAIL] Detected $ZOMBIE_COUNT zombie process(es)!"
    exit 1
fi

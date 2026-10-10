#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

if [ ! -x "$SHELLCRAFT" ]; then
    make
fi

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind not found on system. Skipping valgrind test."
    exit 0
fi

echo "=== Valgrind Memory Leak & File Descriptor Hygiene Test ==="

TMP_INPUT="/tmp/shellcraft_valgrind_input.txt"
VALGRIND_LOG="/tmp/shellcraft_valgrind.log"
rm -f "$TMP_INPUT" "$VALGRIND_LOG"

cat << 'EOF' > "$TMP_INPUT"
pwd > /dev/null
echo "Testing memory discipline"
export TEST_VALGRIND=ACTIVE
printf "3\n2\n1\n" | sort | wc -l > /dev/null
echo "Sub: $(echo inner)" > /dev/null
true && echo "Condition" > /dev/null
stats > /dev/null
exit 0
EOF

valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-fds=yes \
         --error-exitcode=42 \
         --log-file="$VALGRIND_LOG" \
         $SHELLCRAFT < "$TMP_INPUT" > /dev/null

rm -f "$TMP_INPUT"

echo "---------------- Valgrind Summary ----------------"
grep -E "(definitely lost|indirectly lost|possibly lost|ERROR SUMMARY|open file descriptor)" "$VALGRIND_LOG" || cat "$VALGRIND_LOG"
echo "--------------------------------------------------"

if grep -q "0 errors from 0 contexts" "$VALGRIND_LOG" || grep -q "ERROR SUMMARY: 0 errors" "$VALGRIND_LOG"; then
    echo "[PASS] 0 memory errors detected by Valgrind!"
else
    echo "[WARN] Valgrind reported potential errors or leaks. Check log:"
    cat "$VALGRIND_LOG"
    exit 1
fi

rm -f "$VALGRIND_LOG"
echo "Valgrind test completed successfully!"

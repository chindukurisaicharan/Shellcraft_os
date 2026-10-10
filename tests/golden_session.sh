#!/usr/bin/env bash
set -e

SHELLCRAFT="./shellcraft"

echo "=== Golden Session Verification ==="

SESSION_INPUT=$(cat << 'EOF'
echo "GOLDEN_TEST_START"
pwd > /dev/null
echo "G1: Hello Shellcraft"
export GOLDEN_VAR="OS_PROJECT"
echo "G2: Var=$GOLDEN_VAR"
printf "z\nx\ny\n" | sort | tr '\n' ','
echo ""
echo "G4: Redirection" > /tmp/golden_redir.txt
cat < /tmp/golden_redir.txt
rm -f /tmp/golden_redir.txt
true && echo "G5: Logic AND success"
false || echo "G6: Logic OR recovery"
echo "G7: Sub=$(printf 'nested_eval')"
echo "GOLDEN_TEST_END"
exit 0
EOF
)

EXPECTED_OUTPUT=$(cat << 'EOF'
GOLDEN_TEST_START
G1: Hello Shellcraft
G2: Var=OS_PROJECT
x,y,z,
G4: Redirection
G5: Logic AND success
G6: Logic OR recovery
G7: Sub=nested_eval
GOLDEN_TEST_END
EOF
)

ACTUAL_OUTPUT=$(printf "%s\n" "$SESSION_INPUT" | $SHELLCRAFT | grep -A 20 "GOLDEN_TEST_START" | head -n 9)

if [ "$ACTUAL_OUTPUT" = "$EXPECTED_OUTPUT" ]; then
    echo "[PASS] Golden Session matched expected output perfectly!"
else
    echo "[FAIL] Golden Session mismatch!"
    echo "--- Expected ---"
    echo "$EXPECTED_OUTPUT"
    echo "--- Actual ---"
    echo "$ACTUAL_OUTPUT"
    exit 1
fi

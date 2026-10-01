#!/usr/bin/env bash
# Master-Runner für alle Crash Recovery Integrationstests

chmod +x test_cr_crash_recovery.sh test_cr_tamper_shift.sh test_cr_over_threshold.sh

echo "=================================================="
echo " RUNNING CRASH RECOVERY INTEGRATION TEST SUITE "
echo "=================================================="

FAILED=0
FAILED_TESTS=()

run_test() {
    local test_script="$1"

    echo "Running: $test_script"

    if "./$test_script"; then
        echo "PASS: $test_script"
    else
        local exit_code=$?
        echo "FAIL: $test_script (exit code: $exit_code)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$test_script")
    fi

    echo "--------------------------------------------------"
}

run_test "test_cr_crash_recovery.sh"
run_test "test_cr_tamper_shift.sh"
run_test "test_cr_over_threshold.sh"

if [ "$FAILED" -eq 0 ]; then
    echo "ALL INTEGRATION TESTS PASSED SUCCESSFULLY!"
    exit 0
else
    echo "SOME TESTS FAILED! Total failures: $FAILED"
    echo "Failed test scripts:"
    printf ' - %s\n' "${FAILED_TESTS[@]}"
    exit 1
fi
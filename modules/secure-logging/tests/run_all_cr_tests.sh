#!/usr/bin/env bash
# Master-Runner für alle Crash Recovery Integrationstests
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=================================================="
echo " RUNNING CRASH RECOVERY INTEGRATION TEST SUITE "
echo "=================================================="

FAILED=0
FAILED_TESTS=()

run_test() {
    local test_script="$1"

    echo "Running: $test_script"

    if "$SCRIPT_DIR/$test_script"; then
        echo "PASS: $test_script"
    else
        local exit_code=$?
        echo "FAIL: $test_script (exit code: $exit_code)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$test_script")
    fi

    echo "--------------------------------------------------"
}

run_expected_failure() {
    local test_script="$1"

    echo "Running (expected PDF-model limitation): $test_script"

    if "$SCRIPT_DIR/$test_script"; then
        echo "XPASS: $test_script (PDF-model limitation may be fixed)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$test_script (unexpected pass)")
    else
        echo "XFAIL: $test_script (known PDF-model limitation reproduced)"
    fi

    echo "--------------------------------------------------"
}

run_test "test_cr_crash_recovery.sh"
run_test "test_cr_tamper_shift.sh"
run_test "test_cr_over_threshold.sh"
run_test "test_cr_cache_window.sh"
run_test "test_cr_wrong_key.sh"
run_expected_failure "test_cr_partial_write.sh"

if [ "$FAILED" -eq 0 ]; then
    echo "ALL INTEGRATION TESTS PASSED SUCCESSFULLY!"
    exit 0
else
    echo "SOME TESTS FAILED! Total failures: $FAILED"
    echo "Failed test scripts:"
    printf ' - %s\n' "${FAILED_TESTS[@]}"
    exit 1
fi
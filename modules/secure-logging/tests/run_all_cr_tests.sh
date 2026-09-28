#!/usr/bin/env bash
# Master-Runner für alle Crash Recovery Integrationstests

chmod +x test_cr_crash_recovery.sh test_cr_tamper_shift.sh test_cr_over_threshold.sh

echo "=================================================="
echo " RUNNING CRASH RECOVERY INTEGRATION TEST SUITE "
echo "=================================================="

FAILED=0

./test_cr_crash_recovery.sh || FAILED=$((FAILED + 1))
echo "--------------------------------------------------"

./test_cr_tamper_shift.sh || FAILED=$((FAILED + 1))
echo "--------------------------------------------------"

./test_cr_over_threshold.sh || FAILED=$((FAILED + 1))
echo "--------------------------------------------------"

if [ $FAILED -eq 0 ]; then
    echo "ALL INTEGRATION TESTS PASSED SUCCESSFULLY!"
    exit 0
else
    echo "SOME TESTS FAILED! Total failures: $FAILED"
    exit 1
fi
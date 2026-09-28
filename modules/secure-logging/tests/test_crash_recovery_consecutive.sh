#!/bin/bash

#
# File: test_crash_recovery.sh
# Automated crash-recovery test for complete corrupted CR log entries.
#
# Each test starts from the original encrypted log file.
# Complete encrypted entries are overwritten with zeros while the
# file geometry remains unchanged.
#

set -u

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"

ORIGINAL_PLAIN="/tmp/plainlog.txt"
ORIGINAL_ENC="/tmp/plainlog.txt.enc"
KEY_FILE="${TEST_DIR}/master.key"

CORRUPT_TOOL="${HOME}/syslog-ng/build/modules/secure-logging/tests/cr_corrupt_entry"
VERIFIER="${HOME}/syslog-ng/build/modules/secure-logging/crashrecovery/cr_verifier/.libs/cr_verifier"

MAXLOGS=500
BLOCK_SIZE=2112

MAX_CORRUPTED=550

TEST_PREFIX="/tmp/crash_recovery_consecutive"

PASS_COUNT=0
FAIL_COUNT=0

echo "============================================================"
echo " Crash Recovery Automated Test"
echo "============================================================"
echo
echo "Original plaintext : ${ORIGINAL_PLAIN}"
echo "Original encrypted : ${ORIGINAL_ENC}"
echo "Key                : ${KEY_FILE}"
echo "Corrupt tool       : ${CORRUPT_TOOL}"
echo "Verifier           : ${VERIFIER}"
echo "Max corrupted     : ${MAX_CORRUPTED}"
echo

#
# Check required files/tools.
#

if [ ! -f "${ORIGINAL_PLAIN}" ]; then
    echo "ERROR: Missing plaintext file:"
    echo "       ${ORIGINAL_PLAIN}"
    exit 1
fi

if [ ! -f "${ORIGINAL_ENC}" ]; then
    echo "ERROR: Missing encrypted log file:"
    echo "       ${ORIGINAL_ENC}"
    exit 1
fi

if [ ! -f "${KEY_FILE}" ]; then
    echo "ERROR: Missing key file:"
    echo "       ${KEY_FILE}"
    exit 1
fi

if [ ! -x "${CORRUPT_TOOL}" ]; then
    echo "ERROR: Corruption tool is not executable:"
    echo "       ${CORRUPT_TOOL}"
    exit 1
fi

if [ ! -x "${VERIFIER}" ]; then
    echo "ERROR: Verifier is not executable:"
    echo "       ${VERIFIER}"
    exit 1
fi

#
# Determine original file size.
#

ORIGINAL_SIZE=$(stat -c '%s' "${ORIGINAL_ENC}")

if [ "${ORIGINAL_SIZE}" -le 0 ]; then
    echo "ERROR: Invalid original file size."
    exit 1
fi

if [ $((ORIGINAL_SIZE % BLOCK_SIZE)) -ne 0 ]; then
    echo "ERROR: Original encrypted file size is not a multiple of ${BLOCK_SIZE}."
    exit 1
fi

TOTAL_BLOCKS=$((ORIGINAL_SIZE / BLOCK_SIZE))

echo "Original file size : ${ORIGINAL_SIZE} bytes"
echo "Block size         : ${BLOCK_SIZE} bytes"
echo "Total blocks       : ${TOTAL_BLOCKS}"
echo

#
# Calculate original plaintext hash and line count once.
#

ORIGINAL_HASH=$(sha256sum "${ORIGINAL_PLAIN}" | awk '{print $1}')
ORIGINAL_LINES=$(wc -l < "${ORIGINAL_PLAIN}")

echo "Original SHA-256   : ${ORIGINAL_HASH}"
echo "Original lines     : ${ORIGINAL_LINES}"
echo

#
# Test one corruption level.
#
# The corrupted entries are:
#   10, 20, 30, ...
#
# This keeps the corrupted entries separated and makes the test
# deterministic.
#

for ((N=1; N<=MAX_CORRUPTED; N++))
do
    ENC_FILE="${TEST_PREFIX}_${N}.enc"
    RECOVERED_FILE="${TEST_PREFIX}_${N}.recovered.txt"
    VERIFIER_LOG="${TEST_PREFIX}_${N}.verifier.log"

    echo
    echo "------------------------------------------------------------"
    echo "TEST: ${N} corrupted block(s)"
    echo "------------------------------------------------------------"

    #
    # Make sure the requested entries exist.
    #
    LAST_ENTRY=$((N * 10))

    if [ "${LAST_ENTRY}" -ge "${TOTAL_BLOCKS}" ]; then
        echo "STOP: Entry ${LAST_ENTRY} does not exist."
        break
    fi

    #
    # Start every test from the untouched original.
    #
    cp "${ORIGINAL_ENC}" "${ENC_FILE}"

    if [ $? -ne 0 ]; then
        echo "FAIL: Could not create test file."
        FAIL_COUNT=$((FAIL_COUNT + 1))
        break
    fi

    #
    # Corrupt N complete entries.
    #
    CORRUPTION_FAILED=0

    for ((I=1; I<=N; I++))
    do
        ENTRY=$((I * 10))

        echo "Corrupting entry ${ENTRY}..."

        "${CORRUPT_TOOL}" \
            --in "${ENC_FILE}" \
            --entry "${ENTRY}"

        if [ $? -ne 0 ]; then
            echo "FAIL: Could not corrupt entry ${ENTRY}."
            CORRUPTION_FAILED=1
            break
        fi
    done

    if [ "${CORRUPTION_FAILED}" -ne 0 ]; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        rm -f "${ENC_FILE}"
        break
    fi

    #
    # Verify that the file size did not change.
    #
    CORRUPTED_SIZE=$(stat -c '%s' "${ENC_FILE}")

    if [ "${CORRUPTED_SIZE}" -ne "${ORIGINAL_SIZE}" ]; then
        echo "FAIL: File size changed!"
        echo "      Original : ${ORIGINAL_SIZE}"
        echo "      Corrupted: ${CORRUPTED_SIZE}"

        FAIL_COUNT=$((FAIL_COUNT + 1))
        rm -f "${ENC_FILE}"
        break
    fi

    echo "File size unchanged: ${CORRUPTED_SIZE} bytes"

    #
    # Run verifier.
    #
    echo "Running verifier..."

    "${VERIFIER}" \
        --key "${KEY_FILE}" \
        --in "${ENC_FILE}" \
        --out "${RECOVERED_FILE}" \
        --maxlogs "${MAXLOGS}" \
        > "${VERIFIER_LOG}" 2>&1

    VERIFIER_EXIT=$?

    #
    # The verifier may report integrity failure because data was
    # intentionally corrupted. Therefore its exit status alone is
    # NOT used as the recovery criterion.
    #

    if [ ! -f "${RECOVERED_FILE}" ]; then
        echo "FAIL: No recovered file was created."
        FAIL_COUNT=$((FAIL_COUNT + 1))
        break
    fi

    #
    # Extract Gaussian rank if available.
    #

    DETECTED_RANK=$(grep "Detected Rank:" "${VERIFIER_LOG}" \
        | tail -1 \
        | awk '{print $3}')

    if [ -z "${DETECTED_RANK}" ]; then
        DETECTED_RANK="unknown"
    fi

    echo "Detected rank      : ${DETECTED_RANK}"

    #
    # Check SHA-256.
    #

    RECOVERED_HASH=$(sha256sum "${RECOVERED_FILE}" | awk '{print $1}')

    if [ "${ORIGINAL_HASH}" = "${RECOVERED_HASH}" ]; then
        HASH_RESULT="PASS"
    else
        HASH_RESULT="FAIL"
    fi

    echo "SHA-256 comparison : ${HASH_RESULT}"

    #
    # Check line count.
    #

    RECOVERED_LINES=$(wc -l < "${RECOVERED_FILE}")

    if [ "${ORIGINAL_LINES}" -eq "${RECOVERED_LINES}" ]; then
        LINES_RESULT="PASS"
    else
        LINES_RESULT="FAIL"
    fi

    echo "Line count         : ${LINES_RESULT}"
    echo "  Original         : ${ORIGINAL_LINES}"
    echo "  Recovered        : ${RECOVERED_LINES}"

    #
    # Byte/content comparison.
    #

    if diff -q "${ORIGINAL_PLAIN}" "${RECOVERED_FILE}" >/dev/null 2>&1; then
        DIFF_RESULT="PASS"
    else
        DIFF_RESULT="FAIL"
    fi

    echo "Content comparison : ${DIFF_RESULT}"

    #
    # Final result for this recovery level.
    #
    if [ "${HASH_RESULT}" = "PASS" ] &&
       [ "${LINES_RESULT}" = "PASS" ] &&
       [ "${DIFF_RESULT}" = "PASS" ] &&
       [ "${CORRUPTED_SIZE}" -eq "${ORIGINAL_SIZE}" ]; then

        echo
        echo "RESULT: PASS"
        echo "${N} complete corrupted block(s) successfully recovered."

        PASS_COUNT=$((PASS_COUNT + 1))

    else

        echo
        echo "RESULT: FAIL"
        echo "${N} complete corrupted block(s) could NOT be fully recovered."

        FAIL_COUNT=$((FAIL_COUNT + 1))

        echo
        echo "Verifier log:"
        echo "  ${VERIFIER_LOG}"

        echo
        echo "Stopping at first failed recovery level."

        rm -f "${ENC_FILE}"
        break
    fi

    #
    # Keep verifier log and recovered file for inspection.
    # Remove only the encrypted test file to save space.
    #

    rm -f "${ENC_FILE}"

done

echo
echo "============================================================"
echo " Test Summary"
echo "============================================================"
echo
echo "Successful levels : ${PASS_COUNT}"
echo "Failed levels     : ${FAIL_COUNT}"
echo

if [ "${FAIL_COUNT}" -eq 0 ]; then
    echo "Maximum tested recovery level: ${PASS_COUNT} corrupted block(s)"
    echo
    echo "All tested crash-recovery levels passed."
    exit 0
else
    echo "Maximum successful recovery level: $((PASS_COUNT)) corrupted block(s)"
    echo
    echo "First failed recovery level: $((PASS_COUNT + 1)) corrupted block(s)"
    exit 1
fi
#!/bin/bash
set -u

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"

ORIGINAL_PLAIN="/tmp/plainlog.txt"
ORIGINAL_ENC="/tmp/plainlog.txt.enc"
KEY_FILE="${TEST_DIR}/master.key"

CORRUPT_TOOL="${HOME}/syslog-ng/build/modules/secure-logging/tests/cr_corrupt_entry"
VERIFIER="${HOME}/syslog-ng/build/modules/secure-logging/crashrecovery/cr_verifier/.libs/cr_verifier"

MAXLOGS=500
TOTAL_BLOCKS=563

TEST_PREFIX="/tmp/crash_recovery_position"

if [[ ! -f "${ORIGINAL_PLAIN}" ]]; then
    echo "ERROR: original plaintext file not found:"
    echo "  ${ORIGINAL_PLAIN}"
    exit 1
fi

if [[ ! -f "${ORIGINAL_ENC}" ]]; then
    echo "ERROR: original encrypted file not found:"
    echo "  ${ORIGINAL_ENC}"
    exit 1
fi

ORIGINAL_SHA256=$(sha256sum "${ORIGINAL_PLAIN}" | awk '{print $1}')
ORIGINAL_LINES=$(wc -l < "${ORIGINAL_PLAIN}")

echo "============================================================"
echo "Crash-Recovery single-position test"
echo "============================================================"
echo
echo "MAXLOGS:       ${MAXLOGS}"
echo "TOTAL BLOCKS:  ${TOTAL_BLOCKS}"
echo
echo "Original SHA256: ${ORIGINAL_SHA256}"
echo "Original lines:  ${ORIGINAL_LINES}"
echo

RESULT_FILE="${TEST_PREFIX}_results.txt"
: > "${RESULT_FILE}"

PASS_COUNT=0
FAIL_COUNT=0

for ((ENTRY=0; ENTRY<TOTAL_BLOCKS; ENTRY++))
do
    ENC_FILE="${TEST_PREFIX}_${ENTRY}.enc"
    RECOVERED_FILE="${TEST_PREFIX}_${ENTRY}.recovered.txt"
    VERIFIER_LOG="${TEST_PREFIX}_${ENTRY}.verifier.log"

    cp "${ORIGINAL_ENC}" "${ENC_FILE}"

    "${CORRUPT_TOOL}" \
        --in "${ENC_FILE}" \
        --entry "${ENTRY}" \
        >/dev/null 2>&1

    "${VERIFIER}" \
        --key "${KEY_FILE}" \
        --in "${ENC_FILE}" \
        --out "${RECOVERED_FILE}" \
        --maxlogs "${MAXLOGS}" \
        >"${VERIFIER_LOG}" 2>&1

    DETECTED_RANK=$(grep "Detected Rank:" "${VERIFIER_LOG}" \
        | tail -1 \
        | awk '{print $3}')

    RECOVERED_LINES=$(wc -l < "${RECOVERED_FILE}" 2>/dev/null || echo 0)

    RECOVERED_SHA256=$(sha256sum "${RECOVERED_FILE}" 2>/dev/null \
        | awk '{print $1}')

    if [[ "${RECOVERED_SHA256}" == "${ORIGINAL_SHA256}" ]] \
        && [[ "${RECOVERED_LINES}" -eq "${ORIGINAL_LINES}" ]]; then

        RESULT="PASS"
        ((PASS_COUNT++))
    else
        RESULT="FAIL"
        ((FAIL_COUNT++))
    fi

    printf "%3d  rank=%-3s  lines=%-3s  %s\n" \
        "${ENTRY}" \
        "${DETECTED_RANK:-?}" \
        "${RECOVERED_LINES}" \
        "${RESULT}" \
        | tee -a "${RESULT_FILE}"

    rm -f "${ENC_FILE}" "${RECOVERED_FILE}" "${VERIFIER_LOG}"
done

echo
echo "============================================================"
echo "Position test finished"
echo "============================================================"
echo
echo "PASS: ${PASS_COUNT}"
echo "FAIL: ${FAIL_COUNT}"
echo
echo "Results:"
echo "  ${RESULT_FILE}"
echo

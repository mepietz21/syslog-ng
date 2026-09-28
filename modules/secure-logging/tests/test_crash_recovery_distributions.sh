#!/bin/bash
set -u

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"

ORIGINAL_PLAIN="/tmp/plainlog.txt"
ORIGINAL_ENC="/tmp/plainlog.txt.enc"

KEY_FILE="${TEST_DIR}/master.key"
CORRUPT_TOOL="${HOME}/syslog-ng/build/modules/secure-logging/tests/cr_corrupt_entry"
VERIFIER="${HOME}/syslog-ng/build/modules/secure-logging/crashrecovery/cr_verifier/.libs/cr_verifier"

MAXLOGS=500
CORRUPTED=55

TEST_PREFIX="/tmp/crash_recovery_distribution"

declare -a TEST_NAMES=(
    "beginning"
    "middle"
    "end"
    "spaced"
)

echo "============================================================"
echo "Crash-Recovery distribution test"
echo "============================================================"
echo
echo "MAXLOGS:    ${MAXLOGS}"
echo "CORRUPTED:  ${CORRUPTED}"
echo

if [[ ! -x "${CORRUPT_TOOL}" ]]; then
    echo "ERROR: corruption tool not found:"
    echo "  ${CORRUPT_TOOL}"
    exit 1
fi

if [[ ! -x "${VERIFIER}" ]]; then
    echo "ERROR: verifier not found:"
    echo "  ${VERIFIER}"
    exit 1
fi

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

echo "Original SHA256: ${ORIGINAL_SHA256}"
echo "Original lines:  ${ORIGINAL_LINES}"
echo

for TEST_NAME in "${TEST_NAMES[@]}"
do
    ENC_FILE="${TEST_PREFIX}_${TEST_NAME}.enc"
    RECOVERED_FILE="${TEST_PREFIX}_${TEST_NAME}.recovered.txt"
    VERIFIER_LOG="${TEST_PREFIX}_${TEST_NAME}.verifier.log"

    echo
    echo "------------------------------------------------------------"
    echo "TEST: ${TEST_NAME}"
    echo "------------------------------------------------------------"

    cp "${ORIGINAL_ENC}" "${ENC_FILE}"

    case "${TEST_NAME}" in

        beginning)
            echo "Corrupting entries: 0..54"

            for ((ENTRY=0; ENTRY<55; ENTRY++))
            do
                "${CORRUPT_TOOL}" \
                    --in "${ENC_FILE}" \
                    --entry "${ENTRY}" \
                    >/dev/null 2>&1

                if [[ $? -ne 0 ]]; then
                    echo "ERROR: corruption failed for entry ${ENTRY}"
                    exit 1
                fi
            done
            ;;

        middle)
            echo "Corrupting entries: 100..154"

            for ((ENTRY=100; ENTRY<=154; ENTRY++))
            do
                "${CORRUPT_TOOL}" \
                    --in "${ENC_FILE}" \
                    --entry "${ENTRY}" \
                    >/dev/null 2>&1

                if [[ $? -ne 0 ]]; then
                    echo "ERROR: corruption failed for entry ${ENTRY}"
                    exit 1
                fi
            done
            ;;

        end)
            echo "Corrupting entries: 445..499"

            for ((ENTRY=445; ENTRY<=499; ENTRY++))
            do
                "${CORRUPT_TOOL}" \
                    --in "${ENC_FILE}" \
                    --entry "${ENTRY}" \
                    >/dev/null 2>&1

                if [[ $? -ne 0 ]]; then
                    echo "ERROR: corruption failed for entry ${ENTRY}"
                    exit 1
                fi
            done
            ;;

        spaced)
            echo "Corrupting entries: 10,20,...,550"

            for ((I=1; I<=55; I++))
            do
                ENTRY=$((I * 10))

                "${CORRUPT_TOOL}" \
                    --in "${ENC_FILE}" \
                    --entry "${ENTRY}" \
                    >/dev/null 2>&1

                if [[ $? -ne 0 ]]; then
                    echo "ERROR: corruption failed for entry ${ENTRY}"
                    exit 1
                fi
            done
            ;;

    esac

    "${VERIFIER}" \
        --key "${KEY_FILE}" \
        --in "${ENC_FILE}" \
        --out "${RECOVERED_FILE}" \
        --maxlogs "${MAXLOGS}" \
        > "${VERIFIER_LOG}" 2>&1

    VERIFIER_RC=$?

    DETECTED_RANK=$(grep "Detected Rank:" "${VERIFIER_LOG}" \
        | tail -1 \
        | awk '{print $3}')

    BACKSUB=$(grep "BackSubstitution" "${VERIFIER_LOG}" \
        | tail -1)

    RECOVERED_LINES=$(wc -l < "${RECOVERED_FILE}" 2>/dev/null || echo 0)

    if [[ -f "${RECOVERED_FILE}" ]]; then
        RECOVERED_SHA256=$(sha256sum "${RECOVERED_FILE}" | awk '{print $1}')
    else
        RECOVERED_SHA256="FILE_NOT_FOUND"
    fi

    echo
    echo "Verifier return code: ${VERIFIER_RC}"
    echo "Detected Rank:        ${DETECTED_RANK:-NOT_FOUND}"
    echo "BackSubstitution:     ${BACKSUB:-NOT_FOUND}"
    echo "Recovered lines:      ${RECOVERED_LINES}"
    echo "Original lines:       ${ORIGINAL_LINES}"
    echo "Original SHA256:      ${ORIGINAL_SHA256}"
    echo "Recovered SHA256:     ${RECOVERED_SHA256}"

    if [[ "${RECOVERED_SHA256}" == "${ORIGINAL_SHA256}" ]] \
        && [[ "${RECOVERED_LINES}" -eq "${ORIGINAL_LINES}" ]]; then

        echo "RESULT: PASS"

    else

        echo "RESULT: FAIL"
        echo
        echo "Verifier log:"
        grep -E \
            "Detected Rank|BackSubstitution|rank:|res.code|res.success|found=0|tampered|ERROR|WARNING" \
            "${VERIFIER_LOG}" \
            | tail -100
    fi

    rm -f "${ENC_FILE}"
done

echo
echo "============================================================"
echo "Distribution test finished"
echo "============================================================"
echo
echo "Detailed verifier logs:"
for TEST_NAME in "${TEST_NAMES[@]}"
do
    echo "  ${TEST_PREFIX}_${TEST_NAME}.verifier.log"
done

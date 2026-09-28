#!/bin/bash

#
# File: cli46_cr_delete_entry_recovery.sh
#
# Crash Recovery test:
# Simulate an attacker deleting one complete encrypted log entry
# and verify that Crash Recovery can reconstruct the original log.
#

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SUBFOLDER_TEST="test_cr_delete_entry"

COUNT_OF_LOG_LINES=500
DELETE_ENTRY=10

TEST="${SCRIPT_DIR}/${SUBFOLDER_TEST}"

# Prefer installed binaries if BIN is supplied.
# Otherwise use the local build tree.
if [ -z "${BIN:-}" ]; then
    if [ -x "${HOME}/Software/install/bin/cr_logger" ]; then
        BIN="${HOME}/Software/install/bin"
    else
        BIN="${SCRIPT_DIR}/../../../build/modules/secure-logging/crashrecovery/cr_logger"
    fi
fi

CR_LOGGER="${BIN}/cr_logger"
CR_VERIFIER="${BIN}/cr_verifier"
CR_DELETE_ENTRY="${BIN}/cr_delete_entry"

echo "----------------------------------------"
echo "--- Crash Recovery delete-entry test"
echo "----------------------------------------"
echo
echo "SCRIPT_DIR:       ${SCRIPT_DIR}"
echo "TEST:             ${TEST}"
echo "COUNT_OF_LOG_LINES: ${COUNT_OF_LOG_LINES}"
echo "DELETE_ENTRY:     ${DELETE_ENTRY}"
echo "BIN:              ${BIN}"
echo

#
# Check required executables
#
for PROGRAM in "${CR_LOGGER}" "${CR_VERIFIER}" "${CR_DELETE_ENTRY}"; do
    if [ ! -x "${PROGRAM}" ]; then
        echo "ERROR: Required executable not found:"
        echo "       ${PROGRAM}"
        exit 1
    fi
done

#
# Check required input files
#
if [ ! -x "${SCRIPT_DIR}/generate_logs.sh" ]; then
    echo "ERROR: generate_logs.sh not found:"
    echo "       ${SCRIPT_DIR}/generate_logs.sh"
    exit 1
fi

if [ ! -f "${SCRIPT_DIR}/master.key" ]; then
    echo "ERROR: master.key not found:"
    echo "       ${SCRIPT_DIR}/master.key"
    exit 1
fi

#
# Prepare test directory
#
rm -rf "${TEST}"
mkdir -p "${TEST}"

cp "${SCRIPT_DIR}/generate_logs.sh" "${TEST}/"
cp "${SCRIPT_DIR}/master.key" "${TEST}/"

#
# Generate original plaintext log
#
echo "--- Generate plaintext log"

"${TEST}/generate_logs.sh" "${COUNT_OF_LOG_LINES}" \
    > "${TEST}/plainlog.txt"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to generate plaintext log"
    exit 1
fi

#
# Encrypt plaintext log
#
echo "--- Encrypt plaintext log"

"${CR_LOGGER}" \
    --key "${TEST}/master.key" \
    --in "${TEST}/plainlog.txt" \
    --out "${TEST}/plainlog.txt.enc" \
    --maxlogs "${COUNT_OF_LOG_LINES}"

if [ $? -ne 0 ]; then
    echo "ERROR: cr_logger failed"
    exit 1
fi

#
# Create attack copy.
#
# The original encrypted log remains untouched.
#
cp "${TEST}/plainlog.txt.enc" \
   "${TEST}/deleted_entry.enc"

if [ $? -ne 0 ]; then
    echo "ERROR: Could not create attack copy"
    exit 1
fi

#
# Simulate attacker:
# delete one complete encrypted log entry.
#
echo "--- Simulate attacker"
echo "    deleting encrypted entry ${DELETE_ENTRY}"

"${CR_DELETE_ENTRY}" \
    --in "${TEST}/deleted_entry.enc" \
    --entry "${DELETE_ENTRY}"

if [ $? -ne 0 ]; then
    echo "ERROR: cr_delete_entry failed"
    exit 1
fi

#
# Recover the tampered log.
#
echo "--- Run Crash Recovery verifier"

"${CR_VERIFIER}" \
    --key "${TEST}/master.key" \
    --in "${TEST}/deleted_entry.enc" \
    --out "${TEST}/recovered.txt" \
    --maxlogs "${COUNT_OF_LOG_LINES}"

VERIFIER_RESULT=$?

echo
echo "cr_verifier exit code: ${VERIFIER_RESULT}"
echo

#
# Compare original plaintext with recovered plaintext.
#
if [ ! -f "${TEST}/recovered.txt" ]; then
    echo "ERROR: Verifier did not create recovered log"
    exit 1
fi

HASH_ORIGINAL=$(sha256sum "${TEST}/plainlog.txt" | awk '{ print $1 }')
HASH_RECOVERED=$(sha256sum "${TEST}/recovered.txt" | awk '{ print $1 }')

echo "Original SHA256 : ${HASH_ORIGINAL}"
echo "Recovered SHA256: ${HASH_RECOVERED}"
echo

if [ "${HASH_ORIGINAL}" = "${HASH_RECOVERED}" ]; then
    echo "========================================"
    echo "PASS"
    echo "Deleted encrypted entry was recovered."
    echo "Original and recovered logs are identical."
    echo "========================================"
    exit 0
fi

echo "========================================"
echo "FAIL"
echo "Recovered log differs from original."
echo "========================================"

echo
echo "--- File sizes"
wc -c \
    "${TEST}/plainlog.txt" \
    "${TEST}/recovered.txt"

echo
echo "--- Diff"
diff -u \
    "${TEST}/plainlog.txt" \
    "${TEST}/recovered.txt" \
    || true

exit 1

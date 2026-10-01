#!/usr/bin/env bash
# Test 2: Tamper Evidence bei physischer Verschiebung/Truncation
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" 2>/dev/null && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd)"
TEST_CR_BASE_DIR="$CR_BASE_DIR"
TEST_TOP_DIR="$TOP_BUILDDIR"
# shellcheck source=/dev/null
source "$SCRIPT_DIR/cr_test_helpers.sh"
require_bins cr_logger cr_delete_entry cr_verifier

TMP_DIR=$(mktemp -d /tmp/cr_test_shift_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"

MAXLOGS=50
DELETE_INDEX=12

echo "[TEST 2] Generiere Testdaten ($MAXLOGS Zeilen)..."
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none

for i in $(seq 1 $MAXLOGS); do
    echo "Log zeile $i: Tamper Evidence Test" >> "$PLAIN_LOG"
done

"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m $MAXLOGS

ORIGINAL_SIZE=$(stat -c %s "$ENC_LOG")

echo "[TEST 2] Simuliere Angreifer: Physisches Herausschneiden von Block #$DELETE_INDEX..."
"$CR_DELETE" -i "$ENC_LOG" -e $DELETE_INDEX

CURRENT_SIZE=$(stat -c %s "$ENC_LOG")
EXPECTED_SIZE=$((ORIGINAL_SIZE - 2112))
if [ "$CURRENT_SIZE" -ne "$EXPECTED_SIZE" ]; then
    echo "FEHLER: Erwartete Dateigröße $EXPECTED_SIZE, tatsächlich $CURRENT_SIZE" >&2
    exit 1
fi

echo "[TEST 2] Führe cr_verifier aus (Erwartet: Fehler/Abbruch)..."
#set +e
#"$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m $MAXLOGS
#VERIFIER_RC=$?
#set -e

VERIFIER_LOG="$TMP_DIR/verifier.log"
if "$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m "$MAXLOGS" >"$VERIFIER_LOG" 2>&1; then
    VERIFIER_RC=0
else
    VERIFIER_RC=$?
fi


if [ "$VERIFIER_RC" -ne 0 ]; then
    echo "===> PASS: Manipulation erfolgreich erkannt! Verifier schlägt wie erwartet alarm (RC=$VERIFIER_RC)."
    echo "Verifier Log: $VERIFIER_LOG"
    exit 0
else
    echo "===> FAIL: Verifier hat das physische Löschen/Verschieben nicht erkannt!"
    cat "$VERIFIER_LOG"
    exit 1
fi
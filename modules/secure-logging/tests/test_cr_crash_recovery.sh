#!/usr/bin/env bash
# Test 1: In-Place Crash Recovery (Zeroing)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" 2>/dev/null && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd)"
TEST_CR_BASE_DIR="$CR_BASE_DIR"
TEST_TOP_DIR="$TOP_BUILDDIR"
# shellcheck source=/dev/null
source "$SCRIPT_DIR/cr_test_helpers.sh"
require_bins cr_logger cr_corrupt_entry cr_verifier

TMP_DIR=$(mktemp -d /tmp/cr_test_crash_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"

MAXLOGS=100
ZERO_COUNT=4 # Implementation-level approximation of a bounded crash loss.

echo "[TEST 1] Generiere Testdaten ($MAXLOGS Zeilen)..."
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none

for i in $(seq 1 $MAXLOGS); do
    echo "Log zeile $i: Crash Recovery Verification Payload" >> "$PLAIN_LOG"
done

echo "[TEST 1] Verschlüssele Logs via cr_logger..."
"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m $MAXLOGS

echo "[TEST 1] Simuliere begrenzten Crash-Verlust: nulle $ZERO_COUNT feste Slots..."
INDICES="3 27 51 75"
ORIGINAL_SIZE=$(stat -c %s "$ENC_LOG")


for idx in $INDICES; do
    "$CR_CORRUPT" --in "$ENC_LOG" --entry "$idx"
done

CURRENT_SIZE=$(stat -c %s "$ENC_LOG")
if [ "$CURRENT_SIZE" -ne "$ORIGINAL_SIZE" ]; then
    echo "FEHLER: Die Größe der verschlüsselten Datei hat sich nach dem Nulling geändert!" >&2
    exit 1
fi

echo "[TEST 1] Führe cr_verifier aus..."
if "$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m "$MAXLOGS" >/dev/null 2>&1; then
   VERIFIER_RC=0
else
    VERIFIER_RC=$?
fi

echo "[TEST 1] Prüfe Rückgabewert von cr_verifier: $VERIFIER_RC"

if [ "$VERIFIER_RC" -ne 0 ]; then
    echo "===> FAIL: cr_verifier ist mit RC=$VERIFIER_RC fehlgeschlagen!" >&2
    exit 1
fi

echo "[TEST 1] Prüfe Wiederherstellung..."
if diff -u "$PLAIN_LOG" "$DECRYPTED_LOG" > /dev/null; then
    echo "===> PASS: Crash Recovery erfolgreich! Alle $MAXLOGS Zeilen trotz Zeroing rekonstruiert."
    exit 0
else
    echo "===> FAIL: Rekonstruierte Datei weicht vom Original ab!"
    exit 1
fi
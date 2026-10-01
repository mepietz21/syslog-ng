#!/usr/bin/env bash
# Test 3: Überlastung der Recovery-Kapazität (delta > sqrt(n))
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" 2>/dev/null && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd)"
TEST_CR_BASE_DIR="$CR_BASE_DIR"
TEST_TOP_DIR="$TOP_BUILDDIR"
# shellcheck source=/dev/null
source "$SCRIPT_DIR/cr_test_helpers.sh"
require_bins cr_logger cr_corrupt_entry cr_verifier

TMP_DIR=$(mktemp -d /tmp/cr_test_overload_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"

MAXLOGS=64
ZERO_COUNT=9 # m=ceil(64 * THE_C)=72; 9 erasures exceed 8 redundant rows.

echo "[TEST 3] Generiere Testdaten ($MAXLOGS Zeilen)..."
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none

for i in $(seq 1 $MAXLOGS); do
    echo "Log zeile $i: Overload Recovery Test" >> "$PLAIN_LOG"
done

"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m $MAXLOGS

echo "[TEST 3] Nulle $ZERO_COUNT Slots (Implementation-Boundary)..."
INDICES="1 8 15 22 29 36 43 50 57"
for idx in $INDICES; do
    "$CR_CORRUPT" --in "$ENC_LOG" --entry "$idx"
done

echo "[TEST 3] Führe cr_verifier aus (Erwartet: Rangverlust/Fehler)..."
set +e
"$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m "$MAXLOGS" >/dev/null 2>&1
VERIFIER_RC=$?
set -e

if [ $VERIFIER_RC -ne 0 ]; then
    echo "===> PASS: Überlastung erkannt! Verifier meldet Rangverlust/Fehler wie erwartet."
    exit 0
else
    echo "===> FAIL: Verifier behauptet Erfolg, obwohl zu viele Blöcke fehlen!"
    exit 1
fi
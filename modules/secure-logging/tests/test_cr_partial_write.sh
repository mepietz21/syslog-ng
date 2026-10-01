#!/usr/bin/env bash
# PDF-inspired negative test: a partially written fixed-size slot is detected.
# The current verifier requires complete LOG_LEN geometry and does not recover a
# truncated physical tail as the paper's abstract Recover algorithm describes.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TEST_CR_BASE_DIR="$CR_BASE_DIR"
TEST_TOP_DIR="$TOP_BUILDDIR"
source "$SCRIPT_DIR/cr_test_helpers.sh"
require_bins cr_logger cr_verifier

TMP_DIR=$(mktemp -d /tmp/cr_test_partial_write_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"
VERIFIER_LOG="$TMP_DIR/verifier.log"
MAXLOGS=32

printf '[TEST PARTIAL] Erzeuge %d Logzeilen und schneide den letzten Slot ab...\n' "$MAXLOGS"
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none
for i in $(seq 1 "$MAXLOGS"); do
    printf '<134>2026-10-01T12:34:%02dZ host app[42]: Partial write event %d\n' "$((i % 60))" "$i" >> "$PLAIN_LOG"
done

"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m "$MAXLOGS"
ORIGINAL_SIZE=$(stat -c %s "$ENC_LOG")
HALF_SLOT=$((2112 / 2))
truncate -s $((ORIGINAL_SIZE - HALF_SLOT)) "$ENC_LOG"

if "$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m "$MAXLOGS" >"$VERIFIER_LOG" 2>&1; then
    if [ ! -s "$DECRYPTED_LOG" ]; then
        echo "FAIL: Verifier meldete Erfolg, erzeugte aber keine Rekonstruktion" >&2
        exit 1
    fi

    RECOVERED_LINES=$(wc -l < "$DECRYPTED_LOG")
    if [ "$RECOVERED_LINES" -ge "$MAXLOGS" ] || diff -q "$PLAIN_LOG" "$DECRYPTED_LOG" >/dev/null 2>&1; then
        echo "FAIL: Partieller Slot führte zu $RECOVERED_LINES/$MAXLOGS Zeilen; vollständige oder verfälschte Rekonstruktion" >&2
        exit 1
    fi

    echo "PASS: Partieller Slot führte zu einer plausiblen Teilrekonstruktion ($RECOVERED_LINES/$MAXLOGS Zeilen)"
else
    echo "PASS: Partiell geschriebener Slot wurde abgelehnt"
fi

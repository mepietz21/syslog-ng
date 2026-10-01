#!/usr/bin/env bash
# PDF-inspired approximation: lose a bounded set of slots in one crash window.
# The current C implementation has no explicit cs parameter; this tests a fixed
# implementation-level loss budget and must not be read as a formal SLiC test.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TEST_CR_BASE_DIR="$CR_BASE_DIR"
TEST_TOP_DIR="$TOP_BUILDDIR"
source "$SCRIPT_DIR/cr_test_helpers.sh"
require_bins cr_logger cr_corrupt_entry cr_verifier

TMP_DIR=$(mktemp -d /tmp/cr_test_cache_window_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"
MAXLOGS=64
CACHE_WINDOW=4
INDICES="68 69 70 71"

printf '[TEST CACHE] Generiere %d Logzeilen; verliere %d Slots im Crash-Fenster...\n' "$MAXLOGS" "$CACHE_WINDOW"
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none
for i in $(seq 1 "$MAXLOGS"); do
    printf '<134>2026-10-01T12:34:%02dZ host app[42]: Cache window event %d\n' "$((i % 60))" "$i" >> "$PLAIN_LOG"
done

"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m "$MAXLOGS"
ORIGINAL_SIZE=$(stat -c %s "$ENC_LOG")
for index in $INDICES; do
    "$CR_CORRUPT" --in "$ENC_LOG" --entry "$index" >/dev/null
done

if [ "$(stat -c %s "$ENC_LOG")" -ne "$ORIGINAL_SIZE" ]; then
    echo "FAIL: Crash-Fenster hat die Dateigröße verändert" >&2
    exit 1
fi

if ! "$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m "$MAXLOGS" >/dev/null 2>&1; then
    echo "FAIL: Vier Slots im approximierten Crash-Fenster waren nicht rekonstruierbar" >&2
    exit 1
fi

diff -u "$PLAIN_LOG" "$DECRYPTED_LOG" >/dev/null
echo "PASS: Begrenzter Crash-Fenster-Verlust wurde rekonstruiert"

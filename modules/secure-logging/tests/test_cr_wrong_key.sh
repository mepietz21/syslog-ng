#!/usr/bin/env bash
# Authenticated recovery must reject a log verified with the wrong key.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TEST_CR_BASE_DIR="$CR_BASE_DIR"
TEST_TOP_DIR="$TOP_BUILDDIR"
source "$SCRIPT_DIR/cr_test_helpers.sh"
require_bins cr_logger cr_verifier

TMP_DIR=$(mktemp -d /tmp/cr_test_wrong_key_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
WRONG_KEY="$TMP_DIR/wrong.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"
VERIFIER_LOG="$TMP_DIR/verifier.log"
MAXLOGS=32

dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none
dd if=/dev/urandom of="$WRONG_KEY" bs=32 count=1 status=none
for i in $(seq 1 "$MAXLOGS"); do
    printf '<134>2026-10-01T12:34:%02dZ host app[42]: Wrong key event %d\n' "$((i % 60))" "$i" >> "$PLAIN_LOG"
done

"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m "$MAXLOGS"
if "$CR_VERIFIER" -k "$WRONG_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m "$MAXLOGS" >"$VERIFIER_LOG" 2>&1; then
    echo "FAIL: Falscher Master-Key wurde akzeptiert" >&2
    exit 1
fi

echo "PASS: Falscher Master-Key wurde erkannt"

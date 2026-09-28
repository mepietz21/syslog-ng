#!/usr/bin/env bash
# Test 3: Überlastung der Recovery-Kapazität (delta > sqrt(n))
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CR_BASE_DIR="$(cd "$SCRIPT_DIR/../crashrecovery" 2>/dev/null && pwd)"
TOP_BUILDDIR="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd)"

find_bin() {
    local name="$1"
    local env_var_name=$(echo "$name" | tr '[:lower:]' '[:upper:]')
    local env_val="${!env_var_name}"

    # 1. Auswertung gesetzter Umgebungsvariablen (z. B. $CR_LOGGER)
    if [ -n "$env_val" ] && [ -f "$env_val" ] && [ -x "$env_val" ]; then
        echo "$env_val"
        return
    fi

    # 2. Suche im CMake / Autotools build/ Ordner
    if [ -f "$TOP_BUILDDIR/build/modules/secure-logging/crashrecovery/$name/$name" ] && [ -x "$TOP_BUILDDIR/build/modules/secure-logging/crashrecovery/$name/$name" ]; then
        echo "$TOP_BUILDDIR/build/modules/secure-logging/crashrecovery/$name/$name"
    elif [ -f "$TOP_BUILDDIR/build/modules/secure-logging/tests/$name" ] && [ -x "$TOP_BUILDDIR/build/modules/secure-logging/tests/$name" ]; then
        echo "$TOP_BUILDDIR/build/modules/secure-logging/tests/$name"

    # 3. Suche im In-Tree Quellcode-Baum (Standard-Builds)
    elif [ -f "$SCRIPT_DIR/$name" ] && [ -x "$SCRIPT_DIR/$name" ]; then
        echo "$SCRIPT_DIR/$name"
    elif [ -f "$TOP_BUILDDIR/modules/secure-logging/tests/$name" ] && [ -x "$TOP_BUILDDIR/modules/secure-logging/tests/$name" ]; then
        echo "$TOP_BUILDDIR/modules/secure-logging/tests/$name"
    elif [ -f "$CR_BASE_DIR/$name/$name" ] && [ -x "$CR_BASE_DIR/$name/$name" ]; then
        echo "$CR_BASE_DIR/$name/$name"
    elif [ -f "$CR_BASE_DIR/$name" ] && [ -x "$CR_BASE_DIR/$name" ]; then
        echo "$CR_BASE_DIR/$name"

    # 4. System-PATH
    elif command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
    else
        echo ""
    fi
}

CR_LOGGER="$(find_bin cr_logger)"
CR_DELETE="$(find_bin cr_delete_entry)"
CR_VERIFIER="$(find_bin cr_verifier)"

if [ -z "$CR_LOGGER" ] || [ -z "$CR_DELETE" ] || [ -z "$CR_VERIFIER" ]; then
    echo "FEHLER: Eine oder mehere Binärdateien wurden nicht gefunden!" >&2
    echo "  cr_logger:       '${CR_LOGGER:-NICHT GEFUNDEN}'" >&2
    echo "  cr_delete_entry: '${CR_DELETE:-NICHT GEFUNDEN}'" >&2
    echo "  cr_verifier:     '${CR_VERIFIER:-NICHT GEFUNDEN}'" >&2
    exit 1
fi

TMP_DIR=$(mktemp -d /tmp/cr_test_overload_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"

MAXLOGS=64
ZERO_COUNT=30 # 30 >> sqrt(64) = 8 -> Garantiert über der Schwelle

echo "[TEST 3] Generiere Testdaten ($MAXLOGS Zeilen)..."
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none

for i in $(seq 1 $MAXLOGS); do
    echo "Log zeile $i: Overload Recovery Test" >> "$PLAIN_LOG"
done

"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m $MAXLOGS

echo "[TEST 3] Nulle $ZERO_COUNT Blöcke in-place (übersteigt sqrt(n))..."
INDICES=$(shuf -i 0-$((MAXLOGS - 1)) -n $ZERO_COUNT)
for idx in $INDICES; do
    dd if=/dev/zero of="$ENC_LOG" bs=2112 seek="$idx" count=1 conv=notrunc status=none
done

echo "[TEST 3] Führe cr_verifier aus (Erwartet: Rangverlust/Fehler)..."
set +e
"$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m $MAXLOGS
VERIFIER_RC=$?
set -e

if [ $VERIFIER_RC -ne 0 ]; then
    echo "===> PASS: Überlastung erkannt! Verifier meldet Rangverlust/Fehler wie erwartet."
    exit 0
else
    echo "===> FAIL: Verifier behauptet Erfolg, obwohl zu viele Blöcke fehlen!"
    exit 1
fi
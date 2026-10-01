#!/usr/bin/env bash
# Test 1: In-Place Crash Recovery (Zeroing)
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
#CR_DELETE="$(find_bin cr_delete_entry)"
CR_CORRUPT="$(find_bin cr_corrupt_entry)"
CR_VERIFIER="$(find_bin cr_verifier)"

#if [ -z "$CR_LOGGER" ] || [ -z "$CR_DELETE" ] || [ -z "$CR_VERIFIER" ]; then
if [ -z "$CR_LOGGER" ] || [ -z "$CR_CORRUPT" ] || [ -z "$CR_VERIFIER" ]; then
    echo "FEHLER: Eine oder mehrere Binärdateien wurden nicht gefunden!" >&2
    echo "  cr_logger:       '${CR_LOGGER:-NICHT GEFUNDEN}'" >&2
    #echo "  cr_delete_entry: '${CR_DELETE:-NICHT GEFUNDEN}'" >&2
    echo "  cr_corrupt_entry: '${CR_CORRUPT:-NICHT GEFUNDEN}'" >&2
    echo "  cr_verifier:     '${CR_VERIFIER:-NICHT GEFUNDEN}'" >&2
    exit 1
fi

TMP_DIR=$(mktemp -d /tmp/cr_test_crash_XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_KEY="$TMP_DIR/master.key"
PLAIN_LOG="$TMP_DIR/plain.log"
ENC_LOG="$TMP_DIR/test_output.enc"
DECRYPTED_LOG="$TMP_DIR/decrypted.log"

MAXLOGS=100
ZERO_COUNT=4 # 4 < sqrt(100) = 10 -> Sicher im Recovery-Bereich

echo "[TEST 1] Generiere Testdaten ($MAXLOGS Zeilen)..."
dd if=/dev/urandom of="$MASTER_KEY" bs=32 count=1 status=none

for i in $(seq 1 $MAXLOGS); do
    echo "Log zeile $i: Crash Recovery Verification Payload" >> "$PLAIN_LOG"
done

echo "[TEST 1] Verschlüssele Logs via cr_logger..."
"$CR_LOGGER" -k "$MASTER_KEY" -i "$PLAIN_LOG" -o "$ENC_LOG" -m $MAXLOGS

echo "[TEST 1] Simuliere Crash: Nulle $ZERO_COUNT zufällige Blöcke in-place (-z)..."
INDICES=$(shuf -i 0-$((MAXLOGS - 1)) -n $ZERO_COUNT)
ORIGINAL_SIZE=$(stat -c %s "$ENC_LOG")


for idx in $INDICES; do
    dd if=/dev/zero of="$ENC_LOG" bs=2112 seek="$idx" count=1 conv=notrunc status=none
    #"$CR_CORRUPT" --in "$ENC_LOG" --entry "$idx"
done

CURRENT_SIZE=$(stat -c %s "$ENC_LOG")
if ["$CURRENT_SIZE" -ne "$ORIGINAL_SIZE"]; then
    echo "FEHLER: Die Größe der verschlüsselten Datei hat sich nach dem Nulling geändert!" >&2
    exit 1
fi

echo "[TEST 1] Führe cr_verifier aus..."
#"$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m $MAXLOGS
if "$CR_VERIFIER" -k "$MASTER_KEY" -i "$ENC_LOG" -o "$DECRYPTED_LOG" -m $MAXLOGS; then
   VERIFIER_RC=0
else
    VERIFIER_RC=$?
fi

echo "[TEST 1] Prüfe Rückgabewert von cr_verifier: $VERIFIER_RC"

echo "[TEST 1] Prüfe Wiederherstellung..."
if diff -u "$PLAIN_LOG" "$DECRYPTED_LOG" > /dev/null; then
    echo "===> PASS: Crash Recovery erfolgreich! Alle $MAXLOGS Zeilen trotz Zeroing rekonstruiert."
    exit 0
else
    echo "===> FAIL: Rekonstruierte Datei weicht vom Original ab!"
    exit 1
fi
#!/usr/bin/env bash

find_bin() {
    local name="$1"
    local env_var_name
    local env_val

    env_var_name=$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')
    env_val="${!env_var_name:-}"

    if [ -n "$env_val" ] && [ -x "$env_val" ]; then
        printf '%s\n' "$env_val"
        return 0
    fi

    local candidates=(
        "$TEST_TOP_DIR/build/modules/secure-logging/crashrecovery/$name/$name"
        "$TEST_TOP_DIR/build/modules/secure-logging/tests/$name"
        "$TEST_TOP_DIR/modules/secure-logging/tests/$name"
        "$TEST_CR_BASE_DIR/$name/$name"
        "$TEST_CR_BASE_DIR/$name"
    )

    local candidate
    for candidate in "${candidates[@]}"; do
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
        return 0
    fi

    return 1
}

require_bins() {
    local missing=0
    local name
    local path

    for name in "$@"; do
        if path=$(find_bin "$name"); then
            local short_name="${name#cr_}"
            short_name="${short_name%_entry}"
            local variable_name="CR_$(printf '%s' "$short_name" | tr '[:lower:]' '[:upper:]')"
            printf -v "$variable_name" '%s' "$path"
        else
            echo "FEHLER: Binärdatei nicht gefunden: $name" >&2
            missing=1
        fi
    done

    return "$missing"
}

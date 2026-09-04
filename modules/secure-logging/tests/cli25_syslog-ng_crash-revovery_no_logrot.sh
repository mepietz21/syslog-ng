#!/usr/bin/env bash
#############################################################################
# Copyright (c) 2026 Airbus Commercial Aircraft
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# File:   cli20_dest_log_rot_plain_enc.sh
# Date:   2026-05-29
#
# This test is used to test log rotation of syslog-ng with cr_destination.
# No encryption is done. Just logging plain text.
#-----------------------------------------------------------------------

# set -x

VERSION="Version 1.0.0"

# remove path and extension from $0
s=$0
SCRIPTNAME="$(
    b="${s##*/}"
    echo "${b%.*}"
)"
echo "SCRIPTNAME: ${SCRIPTNAME}"

PID=$$
echo "PID: ${PID}"

RANDOM_ID=$(
    /bin/dd if=/dev/urandom bs=1 count=4 2>/dev/null |
        od -An -N4 -tx
)
CLEAN_ID=$(echo "${RANDOM_ID}" | tr -d ' ')

NOW=$(date +%Y-%m-%d_%H%M%S)
echo " "
echo " "
echo "***********************************************************"
echo "*** ${SCRIPTNAME}, ${VERSION}, ${NOW}"
echo "***********************************************************"
echo " "

echo "UNDER CONSTRUCTION. NOT A REAL TEST YET. SOFTWARE TO TEST ALSO UNDER
CONSTRUCTION!"

# -- When there is a parameter given, no matter what, KEEP_DATA is set to TRUE
KEEP_DATA=false
if [[ ! $# -eq 0 ]]; then
    KEEP_DATA=true
fi
echo "KEEP_DATA: ${KEEP_DATA}"

# NOT in POSIX: SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)
# This path must fit the one given to the build system where binaries are provided.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
if [[ ! -e ${PREFIX} ]]; then
    echo "ERROR: Required path PREFIX for installed binaries not found."
    echo "Maybe the project has not yet been built or the build directory was moved or deleted."
    echo "The path to cr_logger and cr_verifier is PREFIX/bin"
    echo "You can try to set PREFIX manually here in the script instead of using get_prefix.sh"
    echo "Example: PREFIX=${HOME}/Software/install"
    echo "FAIL"
    exit 1
fi
HOMESLOGTEST=${SCRIPT_DIR}
UDP_PORT=7777

SFNCONF=syslog-ng-test-crash-recovery.conf
# see also logrotcnt in conf
MAX_LOOP=23

BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
ETC=${PREFIX}/etc
VAR=${PREFIX}/var

SUBFOLDER_TEST="test_slog"
PATH_SUFFIX="${SCRIPTNAME}_${NOW}_${PID}_${CLEAN_ID}"
SUBFOLDER="data"
TEST=/tmp/${SUBFOLDER_TEST}/${SUBFOLDER}_${PATH_SUFFIX}
echo "TEST: ${TEST}"
SESSIONKEY="${TEST}"/currentSession.key
HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
# COPY_TO_HOME_BACKUP="false"
COPY_TO_HOME_BACKUP="true"

# MACADDRESS="01:23:45:67:89:AB"
# SERIALNUMBER="12345678"

# error counter, success when this script returns 0
cnt_error=0

#-----------------------------------------------------------------------
# list current configuration and exit with error
check_script_config() {
    echo "ERROR! Precondition to start failed. Check configuration of $0"
    echo " "

    # Prefix is provided by a script. When this is not working
    # User can try to set it manually its the place where binaries are provided.
    echo "PREFIX: ${PREFIX}"

    echo "BIN: ${BIN}"
    echo "SBIN: ${SBIN}"
    echo "ETC: ${ETC}"
    echo "VAR: ${VAR}"
    echo "TEST: ${TEST}"
    echo "SFNCONF: ${SFNCONF}"
    echo "UDP_PORT: ${UDP_PORT}"

    echo "COPY_TO_HOME_BACKUP: ${COPY_TO_HOME_BACKUP}"
    if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
        echo "HOME_BACKUP: ${HOME_BACKUP}"
    fi

    # stop working. exit now.
    exit 1
}

#-----------------------------------------------------------------------
# -- helper function to check of all files in given array do exist
# Usage: check_missing "path1" "path2" "path3" ...
check_missing() {
    for path in "$@"; do
        # Check if the path does NOT exist (-e works for files and directories)
        if [[ ! -e ${path} ]]; then
            echo "Error: Required path '${path}' not found."
            check_script_config
            # ERROR, check_script_config will exit 1
        fi
    done
    return 0 # SUCESS, all files found
}

#-----------------------------------------------------------------------
# Function to check if a value is a positive integer (including zero).
# Usage: if is_positive_integer "$value"; then ...
# returns 1 when NOT a positive integer
# returns 0 when successfully detected a positive integer
check_positive_integer() {
    # Check if an argument was provided.
    if [[ $# -eq 0 ]]; then
        return 1 # FAILURE, no argument is given
    fi
    local_value="$1"
    # -q suppresses output, and its exit status is the result of the match.
    echo "${local_value}" | grep -q '^[0-9][0-9]*$'
    # The function automatically returns the exit status of the last command (grep).
    # If grep finds a match, it returns 0 (SUCCESS).
    # If grep does not find a match, it returns 1 (FAILURE).
}

#-----------------------------------------------------------------------
# Function to extract the log rotation limit from the syslog-ng configuration file.
# Usage: get_logrotcnt <file_path>
# Output: The integer logrotcont (e.g., 5000) from sysllog-ng.conf is printed to stdout.
# Exit Code: 0 on success (logrotcont found), 1 on failure.
get_logrotcnt() {
    if [[ $# -ne 1 ]]; then
        echo "ERROR: get_logrotcnt requires exactly one argument (file path)." >&2
        return 1
    fi
    local CONF_FILE="$1"
    local CNT_VALUE=""
    if [[ ! -f ${CONF_FILE} ]]; then
        echo "ERROR: Configuration file not found: ${CONF_FILE}" >&2
        return 1
    fi
    # ----------------------------------------------------------------
    # Enclose the entire AWK script in single quotes ('...')
    # to prevent the shell from parsing the inner parentheses and comments.
    # ----------------------------------------------------------------
    CNT_VALUE=$(
        awk -v target_dest="destination d_local_cr {" '
        BEGIN { is_found = 0; }
        
        $0 ~ target_dest { 
            BLOCK_ACTIVE=1;
            next;
        }

        BLOCK_ACTIVE && /};/ {
            BLOCK_ACTIVE=0;
        }

        BLOCK_ACTIVE && /logrotcnt\(/ {
            # These parentheses are now safe because the entire script is quoted.
            sub(/.*logrotcnt\(/, "", $0);   
            sub(/\).*/, "", $0);        
            
            print $0;                   
            is_found = 1;            
            exit 0;                     
        }
        
        END {
            if (is_found) {
                exit 0; 
            } else {
                exit 1; 
            }
        }
    ' "${CONF_FILE}"
    )
    # ----------------------------------------------------------------

    local AWK_STATUS=$?

    if [[ ${AWK_STATUS} -eq 0 ]]; then
        # Success: Print the extracted value to stdout for the caller to capture.
        echo "${CNT_VALUE}"
        return 0
    else
        # Failure: The awk script exited with 1 (limit not found).
        echo "ERROR: Log rotation count (logrotcnt) not found in '${CONF_FILE}'." >&2
        return 1
    fi
}

#-----------------------------------------------------------------------
# Function to check at the end of test, whether log rotation has
# provided expexted log files.
# Usage: check_log_files <path-of-log-files>
# Output: Returns 0 in case of SUCCESS (when NUMBER_OF_LOGFILES are found)
# Exit Code: 0 on success (found all log files), 1 on failure.
# TODO extension as parameter! .enc (encrypted CR) and .log (plain) see also
# mode in conf
check_log_files() {
    local target_dir="$1"
    local extension="$2"
    local count=${NUMBER_OF_LOGFILES}
    local missing=0

    # -- Verify the directory exists first ---
    if [[ ! -d ${target_dir} ]]; then
        echo "[ERROR] Directory does not exist: ${target_dir}"
        return 1
    fi

    echo "Checking ${target_dir} for ${count} log files..."

    # -- Loop through the expected file numbers ---
    for ((i = 1; i <= count; i++)); do
        # Construct the full path
        local file_path="${target_dir}/cr_log_part${i}.${extension}"

        if [[ -f ${file_path} ]]; then
            echo "[OK] Found: ${file_path}"
        else
            echo "[ERROR] Missing: ${file_path}"
            missing=$((missing + 1))
        fi
    done

    # -- Final summary ---
    if [[ ${missing} -eq 0 ]]; then
        echo "Success: All ${count} files are present in ${target_dir}."
        return 0
    else
        echo "Failure: ${missing} file(s) missing in ${target_dir}."
        return 1
    fi
}

#-----------------------------------------------------------------------
# Helper function to stop running syslog-ng background process
stop_syslog() {
    if ! "${SCRIPT_DIR}"/stop_syslog-ng.sh; then
        echo "Warning: Failed to stop syslog-ng"
    fi
}

#-----------------------------------------------------------------------
# Helper function to start syslog-ng background processi
# returns 0 when syslog-ng has been started successfull else 1
start_syslog() {
    if ! "${SCRIPT_DIR}"/start_syslog-ng.sh "${PATH_SUFFIX}"; then
        echo "ERROR: start_syslog: Failed to start syslog-ng"
        return 1
    fi
    # syslog-ng started successfully
    return 0
}

echo " "
"${SCRIPT_DIR}"/get_git_info.sh

mkdir -p "${TEST}"
if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    mkdir -p "${HOME_BACKUP}"
    check_missing "${HOME_BACKUP}"
fi

# -- do initial checks -----
check_missing "${TEST}" "${PREFIX}" "${BIN}" "${SBIN}"

# -- Ensure syslog-ng engined is not running -----
stop_syslog

check_missing "${VAR}" "${ETC}" "${HOMESLOGTEST}" "${SBIN}/syslog-ng" "${SBIN}/syslog-ng-ctl"
check_missing "${BIN}/slogencrypt" "${BIN}/slogverify" "${BIN}/slogkey" "${BIN}/loggen"

# cleanup
if [[ ${KEEP_DATA} == true ]]; then
    echo " "
    echo "INFO: Data from previous test is not deleted!"
    echo "INFO: Keys are re-used!"
    echo " "
else
    # cleanup files from previous tests
    rm -f "${TEST}"/*.key "${TEST}"/*.txt "${TEST}"/*.enc 2>/dev/null
    rm -f "${TEST}"/*.out "${TEST}"/*.log 2>/dev/null
fi

# config working path in syslog-ng.conf

cp -f "${HOMESLOGTEST}/${SFNCONF}" "${TEST}/syslog-ng.conf"
check_missing "${TEST}/syslog-ng.conf" "${SCRIPT_DIR}/update_conf_path.sh"
echo "Update syslog-ng.conf template path .."
RETVALUC=$("${SCRIPT_DIR}/update_conf_path.sh" "${TEST}/syslog-ng.conf" "${PATH_SUFFIX}" "add")
echo "RETVALUC: ${RETVALUC}"

FILE_CONTENT=$(cat "${TEST}/syslog-ng.conf")
echo "${FILE_CONTENT}"

# simple quick check whether udp port is found in conf file
if grep -q "${UDP_PORT}" "${TEST}/syslog-ng.conf"; then
    printf "Found UDP port %s in %s/syslog-ng.conf\n" "${UDP_PORT}" "${TEST}"
else
    printf "ERROR! Not found: UDP port %s in %s/syslog-ng.conf\n" "${UDP_PORT}" "${TEST}" >&2
    exit 1
fi

cp -f "${HOMESLOGTEST}/master.key" "${TEST}/master.key"

# TODO key handling. When time rework key-generation of all scripts. Better
# use prepared keys! Here we need to satify start_syslog-ng.sh which is done,
# by providing a fake host.key.
# This is needed because the script start_syslog-ng.sh is designed to work as
# standalone scrript when there is no test folder nor key and to keep the
# interface simple.
cp -f "${HOMESLOGTEST}/master.key" "${TEST}/host.key"
check_missing "${TEST}/master.key" "${TEST}/host.key"

# -- Check configuration how many log entries are allowed per log file until log rotation ---
#
# get the number of log entries for log rotation (onliner sed version)
# LOGROT_LIMIT=$(sed -n '/destination d_local_cr {/,/};/ {/limit(/s/.*limit(\([0-9]*\)).*/\1/p}' "${TEST}/syslog-ng.conf")
FILE_TO_CHECK="${TEST}/syslog-ng.conf"
LOGROTCNT=$(get_logrotcnt "${FILE_TO_CHECK}")
GET_STATUS=$?
if [[ ${GET_STATUS} -eq 0 ]]; then
    echo "Successfully extracted limit: ${LOGROTCNT}"
else
    echo "Failed to get limit (Exit Status: ${GET_STATUS})"
    exit 1
fi
echo "LOGROTCNT: ${LOGROTCNT}"

# -- check LOGROTCNT ---
if check_positive_integer "${LOGROTCNT}"; then
    echo "OK: LOGROTCNT: '${LOGROTCNT}' is a valid integer value."
else
    echo "Check syslog-ng.conf file destination d_local_cr limit!"
    echo "ERROR: LOGROTCNT: '${LOGROTCNT}' is NOT a valid positive integer (inclusive 0) value."
    exit 1
fi

# -- get number of expected log files ---
if [[ ${LOGROTCNT:-0} -eq 0 ]]; then
    NUMBER_OF_LOGFILES=1
else
    # Calculate number of log files (ceiling division)
    NUMBER_OF_LOGFILES=$(((MAX_LOOP + LOGROTCNT - 1) / LOGROTCNT))
fi
echo "NUMBER_OF_LOGFILES: ${NUMBER_OF_LOGFILES}"

# -- create log entries in loop by UDP -----

#-- check nc variant. On some systems, e.g. Ubuntu the option
# -q is needed even when UDP is used, whereas on Rocky, -q causes an issue.
NC_HAS_Q=0
# -- Check if nc must be provided with q option
if nc -h 2>&1 | grep -q "\-q" || nc --help 2>&1 | grep -q "\-q"; then
    NC_HAS_Q=1
fi

# -- Determine if we are on macOS (due to macOS15 intel machine problem on CI GitHub)
IS_MAC=0
if [[ "$(uname)" == "Darwin" ]]; then
    IS_MAC=1
fi

echo " "
echo "----------------------------------------"
echo "--- create log msg in loop with netcat"
echo "----------------------------------------"
echo " "

# -- start syslog-ng engine -----
#
if ! start_syslog; then
    echo "FAIL"
    exit 1
fi

# -- Trigger syslog-ng by netcat -----
#

i=0
while [[ ${i} -lt ${MAX_LOOP} ]]; do
    i=$((i + 1))

    echo " "
    echo " --- LOOP ${i} of ${MAX_LOOP} ------------"

    LOG_MESSAGE="This is log number ${i} of ${MAX_LOOP} for log rotation test."
    LOG_MESSAGE+="Special chars: <>€£~\$.!@#$%^&*\`§©®™ and some emojis ☀️ 🌍 💡 📚 ☕️ 🍕-${i}"

    echo "${LOG_MESSAGE}" >"${TEST}/plainlog_${i}.txt"
    echo "count of bytes: "
    echo -n "${LOG_MESSAGE}" | wc -c
    echo "Form $0: cat ${TEST}/plainlog_${i}.txt"
    cat "${TEST}/plainlog_${i}.txt"

    #sleep 0.25
    if [[ ${NC_HAS_Q} -eq 1 ]] && [[ ${IS_MAC} -eq 0 ]]; then
        # nc -u -q 3 127.0.0.1 "${UDP_PORT}" <"${TEST}/plainlog_${i}.txt"
        nc -u -q 1 127.0.0.1 "${UDP_PORT}" <"${TEST}/plainlog_${i}.txt"
    else
        nc -u 127.0.0.1 "${UDP_PORT}" <"${TEST}/plainlog_${i}.txt" &
        NC_PID=$!
        # -- Wait for the Intel VM to process the network buffer
        # GitHub CI macOS might need 3 seconds to run stable
        sleep 0.1 # Kill the process to prevent the 10-minute hang
        kill -9 "${NC_PID}" 2>/dev/null || true
        wait "${NC_PID}" 2>/dev/null || true
    fi
    # sleep 0.25

    # now that file handles are closed, save a copy of current file state for
    # each
    echo " "
    echo "ls -alt ${TEST}"
    ls -alt "${TEST}"
    # optical check that key changed
    # echo " "
    # echo "hexdump -C ${SESSIONKEY}"
    # hexdump -C "${SESSIONKEY}"
    # echo " "
    sha256sum "${SESSIONKEY}"

done # while

# -- stop syslog-ng engine -----
stop_syslog

echo "Check whether all ${NUMBER_OF_LOGFILES} log files have been created"
# check_log_files "${TEST}" >/dev/null
check_log_files "${TEST}" "enc"
RETVAL_CL=$?
echo "RETVAL_CL: ${RETVAL_CL}"
if [[ ! ${RETVAL_CL} -eq 0 ]]; then
    cnt_error=$((cnt_error + 1))
fi

echo " "
echo "ls -alt ${TEST}"
ls -alt "${TEST}"
echo " "

echo " "
echo "ls -alt ${TEST}/*.log"
ls -alt "${TEST}"/*.log
echo " "

echo " "
echo "ls ${TEST}/*.enc -alt"
ls "${TEST}"/*enc -alt
echo " "

echo "LOGROTCNT: ${LOGROTCNT}"
echo "NUMBER_OF_LOGFILES: ${NUMBER_OF_LOGFILES}"

if [[ ! ${NUMBER_OF_LOGFILES} -eq 1 ]]; then
    echo " "
    echo "Content of first and last log file will be shown now:"
    echo " "
    echo "${TEST}/cr_log_part1.log"
    FILE_CONTENT=$(cat "${TEST}/cr_log_part1.log")
    echo "${FILE_CONTENT}"
    echo " "
    echo "${TEST}/cr_log_part${NUMBER_OF_LOGFILES}.log"
    FILE_CONTENT=$(cat "${TEST}/cr_log_part${NUMBER_OF_LOGFILES}.log")
    echo "${FILE_CONTENT}"
fi

# -- now check whether first file can be uncrypted ---

# FIXME Bug here or bug in cr_destination src

# TODO ONLY the very last log rot file can be verified successfully!?

i=0
while [[ ${i} -lt ${NUMBER_OF_LOGFILES} ]]; do
    i=$((i + 1))

    PLAIN_FILE="${TEST}/plainlog_${i}.txt"
    ENC_FILE="${TEST}/cr_log_part${i}.enc"
    VERIFY_FILE="${TEST}/cr_log_part${i}_enc_verifier.txt"
    EXPECTED_FILE="${TEST}/cr_log_part${i}_expected.txt"

    echo "LOGROTCNT: ${LOGROTCNT}"

    if [[ ${LOGROTCNT} -eq 0 ]]; then
        START_LOG=1
    else
        START_LOG=$(((i - 1) * LOGROTCNT + 1))
    fi

    END_LOG=$((i * LOGROTCNT))
    if [[ ${END_LOG} -gt ${MAX_LOOP} ]]; then
        END_LOG=${MAX_LOOP}
    fi

    : >"${EXPECTED_FILE}"
    for ((j = START_LOG; j <= END_LOG; j++)); do
        cat "${TEST}/plainlog_${j}.txt" >>"${EXPECTED_FILE}"
    done

    if [[ ! -e ${ENC_FILE} ]]; then
        echo "Error: Required path '${ENC_FILE}' not found."
        cnt_error=$((cnt_error + 1))
    else
        echo "----------------------------------------"
        echo "-- Verifiy log file"
        echo "----------------------------------------"
        echo " "
        if ! "${BIN}/cr_verifier" \
            --key "${TEST}/master.key" \
            --in "${ENC_FILE}" \
            --out "${VERIFY_FILE}" \
            --maxlogs "${LOGROTCNT}"; then
            cnt_error=$((cnt_error + 1))
        fi
        if [[ ! -e ${VERIFY_FILE} ]]; then
            echo "Error: Required path '${VERIFY_FILE}' not found."
            cnt_error=$((cnt_error + 1))
        else
            echo "----------------------------------------"
            echo "-- Check verified log file"
            echo "----------------------------------------"
            echo " "
            HASH1=$(sha256sum "${EXPECTED_FILE}" | awk '{ print $1 }')
            if echo "${HASH1}  ${VERIFY_FILE}" | sha256sum -c - >/dev/null 2>&1; then
                # 'sha256sum -c' exits with 0 (success) if they match
                echo "Original log file is identical to verifier output as expected."
                echo " "
            else
                echo "Error: Decrypted file is different!"
                cnt_error=$((cnt_error + 1))

                echo "cat ${PLAIN_FILE}"
                cat "${PLAIN_FILE}"
                echo " "

                echo "cat ${VERIFY_FILE}"
                cat "${VERIFY_FILE}"
                echo " "

            fi
        fi
    fi

done

# backup copy if configured to do so
if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    # copy from /tmp/test/slog/ to home folder ~/test/<scriptname>/
    cp -R "${TEST}/." "${HOME_BACKUP}/"
fi

# cleanup
if [[ ${KEEP_DATA} == true ]]; then
    echo "Data in /tmp/${SUBFOLDER_TEST}/ will not be deleted!"
else
    rm -rf /tmp/"${SUBFOLDER_TEST}"/"${SUBFOLDER}_${PATH_SUFFIX}"/
fi

echo " "
echo "----------------------------------------"
echo "--- Used binaries"
echo "----------------------------------------"
echo " "
echo "ls -alt ${BIN}/"
ls -alt "${BIN}/"
echo " "
echo "ls -alt ${SBIN}/"
ls -alt "${SBIN}/"
echo " "

sha256sum "${BIN}/cr_logger"
sha256sum "${BIN}/cr_verifier"
sha256sum "${BIN}/slogkey"
sha256sum "${BIN}/slogverify"
sha256sum "${BIN}/slogencrypt"
sha256sum "${BIN}/loggen"
sha256sum "${SBIN}/syslog-ng"
sha256sum "${SBIN}/syslog-ng-ctl"
echo " "

echo "You might want to call this script like:"
echo "$0 2>&1 | tee ./protocol_${SCRIPTNAME}_${NOW}.log"
echo "$0 2>&1 | tee ${HOME}/${SUBFOLDER_TEST}/log/log_${SCRIPTNAME}_${NOW}.log"
mkdir -p "${HOME}/${SUBFOLDER_TEST}"/log
if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    echo "$0 2>&1 | tee ${HOME_BACKUP}/protocol_${SCRIPTNAME}_${NOW}.log"
    echo " "
fi
echo " "

echo "return cnt_error: ${cnt_error}"
#if ((cnt_error == 0)); then
if [[ ${cnt_error} -eq 0 ]]; then
    echo "PASS"
else
    echo "Found ERROR"
    echo "FAIL"
fi

echo " "
echo Done
echo " "
# exit "${cnt_error:-0}"
exit "${cnt_error}"

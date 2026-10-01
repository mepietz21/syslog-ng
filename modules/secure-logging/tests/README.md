# Unit Test of module secure-logging (slog)

For unit tests, the unit test framework Criterion is used.
Secure-logging provides unit tests in file
modules/secure-logging/tests/test_secure_logging.

## Crash-recovery integration scenarios

Run the standalone integration suite from any directory with:

```
modules/secure-logging/tests/run_all_cr_tests.sh
```

The suite currently covers:

* recovery after four complete slot losses,
* detection of a targeted delete-and-shift attack,
* the implementation-specific redundancy boundary (`m = ceil(n * THE_C)`),
* a bounded crash-window approximation with four lost slots, and
* rejection of a wrong master key.

`THE_K` is the number of random positions used per log entry. It is not the
number of recoverable losses. The current implementation does not expose the
paper's `lambda` dummy-entry parameter or cache-size parameter `cs`; therefore
the crash-window tests are implementation-level approximations, not formal
proofs of the SLiC model.

`test_cr_partial_write.sh` is an expected-failure conformance test. It
reproduces the current limitation that a partially written final slot can be
reported as a complete reconstruction. The master runner labels this result
`XFAIL`; it should become `XPASS` only after the verifier is changed and the
test expectation is updated.


# Criterion

https://criterion.readthedocs.io/en/master/intro.html


# Full build process and example configuration

The branches slog-plain-base64 and slog-crash-recovery
do provide build scripts for Autotools and CMake in the root folder
of the syslog-ng project: run_autotool_rebuild.sh and run_cmake_rebuild.sh.

For Crash Recovery one configuration in regard speed optimization is set in the source file
syslog-ng/modules/secure-logging/crashrecovery/cr_verifier/cr_plain_gauss_helper.h

Here it can be decided whether to use special CPU features like SSE2 or AVX2 for Intel x86_64 CPUs.
The default is set to SSE2.

Note: When AVX2 is activated, data to be processed MUST be aligned 32 bytes: __attribute__((aligned(32)));.

But when syslog-ng needs to be compiled for ARM64, like for a raspberry,
the preprocessor CPU_OTHER is set to 1 and CPU_AVX2 and CPU_SSE2 is set to 0.
There are no optimizations for ARM CPUs currently provided.

```
//-- CPU Preprocessors (One and only one must be set to 1 and the others must be set to 0)
//   CPU_AVX2: Intel x86_64 CPU with AVX2 support (fastest, not available on all Intel CPUs)
//   CPU_SSE2: Intel x86_64 CPU with SSE2 support (fast, available on most Intel CPUs)
//   CPU_OTHER: For older Intel CPUs, ARM CPUs e.g. Raspberry Pi (less performant, no optimization)
```
Default CPU configuration when #if defined(__x86_64__)
```
\#define CPU_AVX2  0
\#define CPU_SSE2  1
\#define CPU_OTHER 0
```
For ARM and ARM64 no optimization is provided currently, the preprocessors are 
defined as
```
\#define CPU_AVX2  0
\#define CPU_SSE2  0
\#define CPU_OTHER 1
```

## autogen

The following steps do describe how syslog-ng can be built and how unit
tests are triggered.
From a terminal in the syslog-ng base folder run these commands:
```
./autogen.sh

mkdir build

cd build

../configure --prefix=$HOME/Software/install --enable-debug --enable-slog --enable-manpages --disable-java --disable-python --disable-python-modules --with-ivykis=system --with-jsonc=yes --enable-stomp=no --enable-sql=no

make

make install

make check
```

## cmake

Parallel to the classic build system by autotools (autogen.sh, autoconf), there is also cmake environment available.
Here the make file is created by cmake.
Example: In the syslog-ng source root directory call:

```
mkdir cmake_build

cd cmake_build

cmake \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DIVYKIS_SOURCE=system \
    -DJSONC_SOURCE=system  \
    -DENABLE_SLOG=on \
    -DENABLE_MANPAGES=on   \
    -DENABLE_PYTHON=off   \
    -DENABLE_APPMODEL=on   \
    -DENABLE_PYTHON_MODULES=off \
    -DENABLE_JAVA=off           \
    -DENABLE_PYTHON_JAVA=off    \
    -DENABLE_CLOUD_AUTH=off     \
    -DENABLE_CLOUD_AUTH_CURL=off   \
    -DENABLE_AZURE_AUTH_HEADER=off \
    -DENABLE_STOMP=off             \
    -DENABLE_EXAMPLE_FILTERX_FUNC=off \
    -DENABLE_EXAMPLE_MODULES=off      \
    -DENABLE_XML=off      \
    -DENABLE_STARDATE=off      \
    -DCMAKE_BUILD_TYPE=Debug          \
    -DCMAKE_INSTALL_PREFIX=$HOME/Software/install \
    ..

make

make install

make check
```

## make install

With **make install** a folder structure is created inside the folder, given by
the configure script by option **--prefix** where build artefacts are copied into.

See configure example above:
  --prefix=$HOME/Software/install

## make check

With **make check** the unit tests are executed.
The unit tests do use temporarly folders and files that are deleted at the end
of each test.

Criterion unit tests allow to log some extra information related to a test.
Depending on the build tools the test log is located in a specific location.

Assume that the root source folder is ~/Software/syslog-ng/ and the build
folder set by the build script is ~/Software/syslog-ng/build/ the test log
can be found here:

### automake base build:
~/Software/syslog-ng/build/modules/secure-logging/tests
cat test_secure_logging.log

### cmake base build:
~/Software/syslog-ng/build/Testing/Temporary
cat LastTest.log



# Test of Command Line Interface (CLI) tools

Besides Criterion unit tests that are executed when **make check** is called,
some CLI tools can only be tested with Linux Bash scripts.
The binaries used in the shell scripts are to be updated by **make install** first.

Each test script uses temporarily the folder /tmp/test_slog/data_<unique-suffix>/ and
all data is deleted at the end of each test.
The <unique-suffix> is generated by each script to ensure parallel working in
CI environment by unique data storage.

In case the syslog-ng daemon is tested, the syslog-ng configuration is given by a
template file which is modified and copied as syslog-ng.conf into the unique
test folder.
syslog-ng.conf templates:
- syslog-ng-test-plain-base64-udp-nc.conf
- syslog-ng-test-plain-direct-udp-nc.conf
- slog-ng-test-udp-nc.conf

In case Crash Recovery is tested, in the syslog-ng.conf file a destination with
log rotation is used instead of the template from classic secure-logging.
See syslog-ng.conf templates:
- syslog-ng-test-crash-recovery.conf
- syslog-ng-test-logrot-crash-recovery_cli20.conf
- syslog-ng-test-logrot-crash-recovery_cli26.conf

When needed for debugging purpose, the content of the test folder can be copied
into into the users home folder, see script variables HOME_BACKUP and
COPY_TO_HOME_BACKUP (existing data is deleted or overwritten without asking).



## cli11_enc_syslog_verify_n.sh
Smoke test of CLI tools **syslog-ng**, **syslog-ng-cli**, **slogkey**
and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: enc

## cli12v1_direct_crypt_verify_i.sh
Smoke test of CLI tools **slogkey**, **slogencrypt** and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: direct (plain)

## cli12v2_base64_crypt_verify_i.sh
Smoke test of CLI tools **slogkey**, **slogencrypt** and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: base64 (plain)

## cli12v3_direct_ivtag_crypt_verify_i.sh
Smoke test of CLI tools **slogkey**, **slogencrypt** and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: direct (plain)
Test detection of tampering IV-TAG.

## cli12v4_direct_msg_crypt_verify_i.sh
Smoke test of CLI tools **slogkey**, **slogencrypt** and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: direct (plain)
Test detection of tampering log message.

## cli12v5_enc_crypt_verify_i.sh
Smoke test of CLI tools **slogkey**, **slogencrypt** and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: enc (encrypted, like originally secure-logging)
Test detection of tampering base64 incoded encrypted IV-TAG and log message.

## cli13_plain_loggen_verify_n.sh
Smoke test of CLI tool **loggen**, which uses **syslog-ng** and **syslog-ng-cli**.
Keys and log entries are generated by the test script.

## cli14_plain_loggen_file_verify_n.sh
Smoke test like cli03_loggen.sh but using a user prepared file as input for log entries.
Note: The path of the file must be provided in the script and the file must be
compliant to RFC5424.
There is a small log file provided that will be used automatically.
PAYLOAD=payload_utf8_2048.txt
PAYLOAD_FILE=${TEST}/${PAYLOAD}

## cli15_multiline_all_logmodes_crypt_verify_n.sh
Smoke test of CLI tools **slogkey**, **slogencrypt** and **slogverify**.
Keys and log entries are generated by the test script.
Iterates all three log modes (direct|base64|enc)

## cli15_multiline_all_logmodes_crypt_verify_n_tamper.sh
Like above but with tampering and its detection.

## cli16_plain_syslog_verify_n.sh
Smoke test of CLI tools **syslog-ng**, **syslog-ng-cli**, **slogkey**
and **slogverify**.
Keys and log entries are generated by the test script.
Logmode: enc
Same as cli11_enc_syslog_verify_n.s but here, syslog-ng daemon is not restartet after each log message.

## cli19_plain_valgrind.sh
Run CLI tools **slogkey**, **slogencrypt** and **slogverify** in context of
valgrind to detect memory leaks.
Keys and log entries are generated by the test script.

## cli20_logrot_syslog-ng_crash-recovery.sh
Script to test log rotation with destination from crash recovery module.
Currently configured with  n = 1024 which means each encrypted log
contains 1024 log lines and a new log file is created for the nex 1024 logs.
Note: This test might takes long, e.g. 1h, depends on the used netcat version.
On Rocky it takes 80 seconds, on Debian based systems it takes 3600 seconds.

## cli21_crl_crv.sh
Script to run cr_logger and cr_verifier with a generated small example plain
log file, to test whether Crash Recovery logging and verifying is basically working.

## cli22_crl.sh
Script to trigger cr_logger with a user defined plain log file.

## cli23_crv.sh
Script to trigger cr_verifier with a user defined encrypted log file from cr_logger.
The encrypted log file can be tampered to check whether some bit flips are
detetected and whether the original plain log file can be restored.

## cli24_crl_crv_valgrind.sh
Valgrind for standalone Crash Recovery tools cr_logger and cr_verifier.

## cli25_syslog-ng_crash-revovery_no_logrot.sh
Test and configuration that logging takes place without log rotation, so
the point of log rotation is not reached due small number of logs.

## cli26_logrot_syslog-ng_crash-recovery.sh
Quick test with Log rotation size

## cli27_bin.sh
Script to collect information about build process and binaries.

## cli42_PROBLEM_LOOP_KEY_WHEN_MULITILINE.sh
Demonstration that a multiline file can be processed only once.
Otherwise verification is not possible.

## cli43_enc_syslog_verify_n_fallback_logmode.sh
Use binaries that do support logmode (direct|base64|enc) but the logmode
is not given as arguemnt for cli tools or syslog-ng daemon is getting an old
syslog-ng.conf without logmode argument in template (old classic secure-logging).
This test shall prove that the default logmode enc is used automatically and silent.
syslog-ng is used wit old syslog-ng.conf and slogverify is used in normal mode.

## cli44_crypt_verify_i_valgrind_fallback_logmode.sh
Use binaries that do support logmode (direct|base64|enc) but the logmode
is not given as arguemnt for cli tools slogencrypt and slogverify.
This test shall prove that the default logmode enc is used automatically and silently.
slogverify is used in itertive mode.

## Helper scripts

### get_git_info.sh
Shows current git branch state.

### get_prefix.sh
Script to get the root path of the installation directory.
When project has been build and **make install** has been executed, the binaries are available
in <path_by_get_prefix.sh>/bin or <path_by_get_prefix.sh>/sbin.
This script is called in test scripts.
Note: In case only binaries are provided to the system, there will be no build folder and
a PREFIX won't be found by the script. Therefore the root path of installed binaries can be set in the
script get_prefix.sh and test scripts can be used without the need of executing the build process.
```
...
# DO_USE_HARDCODED_PATH="false"
DO_USE_HARDCODED_PATH="true"

if [ ${DO_USE_HARDCODED_PATH} = "true" ]; then
    # PREFIX=/home/raspi/Software/install
    PATH_PREFIX_VALUE="/home/raspi/Software/install"
    echo "${PATH_PREFIX_VALUE}"
else
...
```



### update_conf_path.sh
For each test a syslog-ng.conf file is copied into the test unique test folder inside
/tmp/test_slog/date_<unique-suffix>.
syslog-ng.conf, based on the template syslog-ng-test-udp-nc.conf, gets the corresponding unique path
by updating its variable @define mypath "/tmp/test_slog/data" into "/tmp/test_slog/data_<unique-suffic>".
The update of syslog-ng.conf is done test script internally by update_conf_path.sh.

### runall.sh
Executes a list of shell scripts.

### loop_cli_x.sh
For test of test scripts to run them several times.

### start_syslog-ng.sh
Script to start syslog-ng daemon, called also in other test scripts.

### stop_syslog-ng.sh
Script to stop syslog-ng daemon, called also in other test scripts.

### tamper_b64.sh
Script to tamper a base64 string.


## Data input files

### inputfile_multiline.txt
Example text file used used for slogenc.
The multiline text file does also contain empty lines for test.
Note: In the original secure-logging, empty lines or multiline text files where
not allowed.


### payload.txt
Prepeard RFC5424 data used as for log messages in context of tool logen.

### payload_utf8_1024.txt
Prepeard 1024 octets long data used as for log messages in context of tool logen.

### payload_utf8_2048.txt
Prepeard 2048 octets long data data used as for log messages in context of tool logen.


# Criterion Unit Test

test_secure_logging.c


# Aliasse

Before build and commit, alias **stylesl** should be called.

There a 2 build scripts provided in the root folder:
**run_auto_rebuild.sh** and **run_cmake_rebuild.sh**.
These can be wrapped into aliasse to call them from everywhere in the
file system, see aliasse **runauto** and **runcmake**.

Once a complete build has been executed, for small code changes there is no
need to rebuild all.
Just change in the build directoy, see alias **goslb**, and type directly make or make install or
make check.


```

#-----------------------------------------------------------------------
# syslog-ng
#-----------------------------------------------------------------------

#-- The following paths might need to be adjusted ---

SYSLOG_DIR="$HOME/Software/syslog-ng"
export SYSLOG_DIR

TESTBAK_DIR="$HOME/test_slog"
export TESTBAK_DIR

SW_INSTALL_DIR="$HOME/Software/install"
export SW_INSTALL_DIR

BUILD_LOG_DIR="$HOME/backup/05_build_log"
export BUILD_LOG_DIR



#-- Define the base LINTFLAGS ---
export UBUNTU_LINTFLAGS="-I/usr/include \
-I${SYSLOG_DIR}/lib \
-I${SYSLOG_DIR}/modules/secure-logging \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_common \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_destination \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_logger \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_verifier \
-I${SYSLOG_DIR}/lib/eventlog/src \
-I${SW_INSTALL_DIR}/include/syslog-ng \
-I/usr/include/glib-2.0 \
-I/usr/include/x86_64-linux-gnu \
-I/usr/include/x86_64-linux-gnu/bits \
-I/usr/include/x86_64-linux-gnu/bits/types \
-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
-I/usr/lib/gcc/x86_64-linux-gnu/13/include \
"
export ROCKY_LINTFLAGS="-I/usr/include \
-I${SYSLOG_DIR}/lib \
-I${SYSLOG_DIR}/modules/secure-logging \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_common \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_destination \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_logger \
-I${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_verifier \
-I${SYSLOG_DIR}/lib/eventlog/src \
-I${SW_INSTALL_DIR}/include/syslog-ng \
-I/usr/bin/gcc \
-I/usr/include/glib-2.0 \
-I/usr/include/glib-2.0/glib \
-I/usr/lib64/glib-2.0/include \
-I/usr/include/json-c \
-I/usr/lib/clang/21/include \
-I/usr/lib/gcc/x86_64-redhat-linux/11/include \
"

export LINTFLAGS=${ROCKY_LINTFLAGS}

#-- Cppcheck alias with short file name and timestamped output ---
cppcheck_test() {
    local filename="$1"
    if [ ! -f "${filename}" ]; then
        echo "Error: File '${filename}' does not exist." >&2
        return 1
    fi
    local shortname=$(basename "$filename")
    local basename="${shortname%.*}"
    mkdir -p "${TESTBAK_DIR}/cppcheck"
    local NOW=$(date +%Y-%m-%d_%H%M_%S)
    cppcheck -v \
    -Dgboolean=_Bool \
    -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68 \
    -DOPENSSL_VERSION_NUMBER=0x30000000L \
    -D__x86_64__ \
    ${LINTFLAGS} \
    --suppress=unusedFunction \
    --addon=misra \
    --inconclusive \
    --enable=all \
    "${filename}" 2>&1 | tee -a "${TESTBAK_DIR}/cppcheck/cppcheck_${basename}_${NOW}.txt"
}
alias runlint='cppcheck_test'

# Usage: $ runlint <file-to-check>
# Example: check slog.c:
# cd ~/Software/syslog-ng/modules/secure-logging
# runlint ./slog.c


#-- Helper for alias blogvim ---
lint_open_newest() {
    local target_dir="${TESTBAK_DIR}/cppcheck"
    if [ ! -d "${target_dir}" ]; then
        echo "Error: Directory ${target_dir} does not exist."
        return 1
    fi
    (
        cd "${target_dir}" || exit
        local newest_file=$(ls -t cppcheck*.txt 2>/dev/null | head -n 1)
        if [ -n "${newest_file}" ]; then
            vim "${newest_file}"
        else
            echo "No cppcheck file found in ${target_dir}"
        fi
    )
}
alias vimlint='lint_open_newest'

# Usage: vimlint
# Notes:
# - After runlint <file> has been executed, might take some time, the last
# output can be viewed with vim.
# - The most warnings are not relevant. But sometimes some are important.
# - The purpose of all this is not only to detect hidden errors also to save
#   effort on pull requests.


# -- Crash Recovery Standalone -----
# --- small log file with 1000 lines
alias crlogs='${SW_INSTALL_DIR}/bin/cr_logger \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg1000.txt" \
--out "${TESTBAK_DIR}/cr/msg1000.enc" \
--maxlogs 1000'

alias trace_crlogs='strace -x -e trace=read,write,fsync,file,network -o ${TESTBAK_DIR}/cr/analysis_l_$(date +"%Y-%m-%d_%H-%M-%S").log \
${SW_INSTALL_DIR}/bin/cr_logger \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg1000.txt" \
--out "${TESTBAK_DIR}/cr/msg1000.enc" \
--maxlogs 1000'

alias crverifys='${SW_INSTALL_DIR}/bin/cr_verifier \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg1000.enc" \
--out "${TESTBAK_DIR}/cr/msg1000_verifier.txt" \
--maxlogs 1000'

alias trace_crverifys='strace -x -e trace=read,write,fsync,file,network -o ${TESTBAK_DIR}/cr/analysis_v_$(date +"%Y-%m-%d_%H-%M-%S").log \
${SW_INSTALL_DIR}/bin/cr_verifier \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg1000.enc" \
--out "${TESTBAK_DIR}/cr/msg1000_verifier.txt" \
--maxlogs 1000'

# -- current test

alias crlog='${SW_INSTALL_DIR}/bin/cr_logger \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg17937.txt" \
--out "${TESTBAK_DIR}/cr/msg17937.enc" \
--maxlogs 17937'

alias crverify='${SW_INSTALL_DIR}/bin/cr_verifier \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg17937.enc" \
--out "${TESTBAK_DIR}/cr/msg17937_verifier.txt" \
--maxlogs 17937'

# --- larger log file with 6696 lines
alias crlogx='${SW_INSTALL_DIR}/bin/cr_logger \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg6696.txt" \
--out "${TESTBAK_DIR}/cr/msg6696.enc" \
--maxlogs 6696'

alias crverifyx='${SW_INSTALL_DIR}/bin/cr_verifier \
--key "${TESTBAK_DIR}/cr/master.key" \
--in "${TESTBAK_DIR}/cr/msg6696.enc" \
--out "${TESTBAK_DIR}/cr/msg6696_verifier.txt" \
--maxlogs 6696'


# alias goslroot='cd $HOME/Software/syslog-ng'
alias goslroot='cd ${SYSLOG_DIR}'

alias goslb='cd ${SYSLOG_DIR}/build'
alias goslbmslt='cd ${SYSLOG_DIR}/build/modules/secure-logging/tests'

alias gosl='cd ${SYSLOG_DIR}/modules/secure-logging'
alias gosle='cd ${SYSLOG_DIR}/modules/secure-logging/slogencrypt'
alias goslv='cd ${SYSLOG_DIR}/modules/secure-logging/slogverify'
alias goslk='cd ${SYSLOG_DIR}/modules/secure-logging/slogkey'
alias goslt='cd ${SYSLOG_DIR}/modules/secure-logging/tests'
alias goslcr='cd ${SYSLOG_DIR}/modules/secure-logging/crashrecovery'
alias goslcrc='cd ${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_common'
alias goslcrl='cd ${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_logger'
alias goslcrv='cd ${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_verifier'
alias goslcrd='cd ${SYSLOG_DIR}/modules/secure-logging/crashrecovery/cr_destination'

alias gosw='cd $HOME/Software'
alias goi='cd ${SW_INSTALL_DIR}'
alias gobin='cd ${SW_INSTALL_DIR}/bin'
alias gosbin='cd ${SW_INSTALL_DIR}/sbin'
alias gobackup='cd $HOME/backup'
alias goblog='cd "${BUILD_LOG_DIR}"'

alias gotest='cd ${TESTBAK_DIR}'
alias gotest11='cd ${TESTBAK_DIR}/cli11_enc_syslog_verify_n'
alias gotest12v1='cd ${TESTBAK_DIR}/cli12v1_direct_crypt_verify_i'
alias gotest12v2='cd ${TESTBAK_DIR}/cli12v2_base64_crypt_verify_i'
alias gotest12v3='cd ${TESTBAK_DIR}/cli12v3_direct_ivtag_crypt_verify_i'
alias gotest12v4='cd ${TESTBAK_DIR}/cli12v4_direct_msg_crypt_verify_i'
alias gotest12v5='cd ${TESTBAK_DIR}/cli12v5_enc_crypt_verify_i'
alias gotest13='cd ${TESTBAK_DIR}/cli13_plain_loggen_verify_n'
alias gotest14='cd ${TESTBAK_DIR}/cli14_plain_loggen_file_verify_n'
alias gotest15='cd ${TESTBAK_DIR}/cli15_multiline_all_logmodes_crypt_verify_n'
alias gotest15t='cd ${TESTBAK_DIR}/cli15_multiline_all_logmodes_crypt_verify_n_tamper'
alias gotest16='cd ${TESTBAK_DIR}/cli16_plain_syslog_verify_n'
alias gotest19='cd ${TESTBAK_DIR}/cli19_plain_valgrind'
alias gotest20='cd ${TESTBAK_DIR}/cli20_logrot_syslog-ng_crash-recovery'
alias gotest21='cd ${TESTBAK_DIR}/cli21_crl_crv'
alias gotest22='cd ${TESTBAK_DIR}/cli22_crl'
alias gotest23='cd ${TESTBAK_DIR}/cli23_crv'
alias gotest24='cd ${TESTBAK_DIR}/cli24_crl_crv_valgrind'
alias gotest25='cd ${TESTBAK_DIR}/cli25_syslog-ng_crash-revovery_no_logrot'
alias gotest26='cd ${TESTBAK_DIR}/cli26_logrot_syslog-ng_crash-recovery'
alias gotest42='cd ${TESTBAK_DIR}/cli42_PROBLEM_LOOP_KEY_WHEN_MULITILINE'

alias gotestcr='cd ${TESTBAK_DIR}/cr'
alias gotestlog='cd ${TESTBAK_DIR}/log'

alias gocoredump='cd /var/lib/systemd/coredump'
alias gogdb='cd $HOME/Software/GDB_COREDUMP'
alias gotesttcr='cd /tmp/test_slog/cr'
alias goval='cd /tmp/test_slog/valgrind_reports'

# The build script were moved into the test folder and
# modified to work from there.

alias runauto='(
 cd ${SYSLOG_DIR}/modules/secure-logging/tests/ &&
 ${SYSLOG_DIR}/modules/secure-logging/tests/run_autotools_rebuild.sh
)'

alias runcmake='(
 cd ${SYSLOG_DIR}/modules/secure-logging/tests/ &&
 ${SYSLOG_DIR}/modules/secure-logging/tests/run_cmake_rebuild.sh
)'

alias runclean='(
 cd ${SYSLOG_DIR}/modules/secure-logging/tests/ &&
 ${SYSLOG_DIR}/modules/secure-logging/tests/run_clean.sh
)'

alias stylesl='(
  cd ${SYSLOG_DIR} &&
  ${SYSLOG_DIR}/scripts/style-checker.sh format
)'



# move old logs into the ./old/ folder
alias blogmv='(
  cd "${BUILD_LOG_DIR}" &&
  mkdir -p "${BUILD_LOG_DIR}"/old/ &&
  mv "${BUILD_LOG_DIR}"/*.txt "${BUILD_LOG_DIR}"/old/ &&
  ls -alt "${BUILD_LOG_DIR}"
)'

# Helper for alias blogvim
blog_open_newest() {
    local target_dir="${BUILD_LOG_DIR}"
    if [ ! -d "${target_dir}" ]; then
        echo "Error: Directory ${target_dir} does not exist."
        return 1
    fi
    (
        cd "${target_dir}" || exit
        local newest_file=$(ls -t *_make.txt 2>/dev/null | head -n 1)
        if [ -n "${newest_file}" ]; then
            vim "${newest_file}"
        else
            echo "No files ending in _make.txt found in ${target_dir}"
        fi
    )
}

alias vimblog='blog_open_newest'



#-- Criterion Unit Tests (CMake and Autotools use various folders and files) ---

## CMake based 'make check'
alias gocmaketest='cd ${SYSLOG_DIR}/build/Testing/Temporary'
alias vimcmaketest='vim ${SYSLOG_DIR}/build/Testing/Temporary/LastTest.log'

## Autotools based 'make check'
alias goautotest='cd ${SYSLOG_DIR}/build/modules/secure-logging/tests'
alias vimautotest='vim ${SYSLOG_DIR}/build/modules/secure-logging/tests/test_secure_logging.log'


#-----------------------------------------------------------------------
# End of aliasse for syslog-ng dev
#-----------------------------------------------------------------------

```


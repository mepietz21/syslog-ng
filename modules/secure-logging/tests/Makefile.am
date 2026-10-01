# File: syslog-ng/modules/secure-logging/tests/Makefile.am

if ENABLE_SLOG

# 1. Nur echte Test-Suites in TESTS aufnehmen (keine Hilfswerkzeuge)
modules_secure_logging_tests_TESTS = \
	modules/secure-logging/tests/test_secure_logging \
	modules/secure-logging/tests/test_cr_common \
	modules/secure-logging/tests/test_cr_destination \
	modules/secure-logging/tests/test_cr_logger \
	modules/secure-logging/tests/test_cr_verifier

# 2. Unit-Tests zu check_PROGRAMS hinzufügen; Hilfswerkzeuge nur bauen.
check_PROGRAMS += \
	${modules_secure_logging_tests_TESTS}

noinst_PROGRAMS += \
	modules/secure-logging/tests/cr_delete_entry \
	modules/secure-logging/tests/cr_corrupt_entry

# --- General Test CPPFLAGS / CFLAGS / LDADD ---

modules_secure_logging_tests_test_secure_logging_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_logger \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_destination \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_verifier

modules_secure_logging_tests_test_secure_logging_CFLAGS = \
	$(AM_CFLAGS) \
	$(TEST_CFLAGS)

modules_secure_logging_tests_test_secure_logging_LDADD = \
	$(TEST_LDADD) \
	modules/secure-logging/crashrecovery/cr_logger/libcr_logger.la \
	modules/secure-logging/crashrecovery/cr_verifier/libcr_verifier.la \
	-dlpreopen ${top_builddir}/modules/secure-logging/libsecure-logging.la \
	-dlpreopen ${top_builddir}/modules/syslogformat/libsyslogformat.la

# --- Helper: cr_delete_entry ---

modules_secure_logging_tests_cr_delete_entry_SOURCES = \
	modules/secure-logging/tests/cr_delete_entry.c

modules_secure_logging_tests_cr_delete_entry_CPPFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CPPFLAGS)

modules_secure_logging_tests_cr_delete_entry_CFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CFLAGS)

modules_secure_logging_tests_cr_delete_entry_LDADD = \
	modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
	$(TEST_LDADD)

# --- Helper: cr_corrupt_entry ---

modules_secure_logging_tests_cr_corrupt_entry_SOURCES = \
	modules/secure-logging/tests/cr_corrupt_entry.c

modules_secure_logging_tests_cr_corrupt_entry_CPPFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CPPFLAGS)

modules_secure_logging_tests_cr_corrupt_entry_CFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CFLAGS)

modules_secure_logging_tests_cr_corrupt_entry_LDADD = \
	modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
	$(TEST_LDADD)

# --- Standard Unit Tests ---

modules_secure_logging_tests_test_cr_common_CPPFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CPPFLAGS)

modules_secure_logging_tests_test_cr_common_CFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CFLAGS)

modules_secure_logging_tests_test_cr_common_LDADD = \
	$(modules_secure_logging_tests_test_secure_logging_LDADD)

modules_secure_logging_tests_test_cr_destination_CPPFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CPPFLAGS)

modules_secure_logging_tests_test_cr_destination_CFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CFLAGS)

modules_secure_logging_tests_test_cr_destination_LDADD = \
	$(modules_secure_logging_tests_test_secure_logging_LDADD) \
	modules/secure-logging/crashrecovery/cr_destination/libcr_destination.la

modules_secure_logging_tests_test_cr_logger_CPPFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CPPFLAGS)

modules_secure_logging_tests_test_cr_logger_CFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CFLAGS)

modules_secure_logging_tests_test_cr_logger_LDADD = \
	$(modules_secure_logging_tests_test_secure_logging_LDADD)

modules_secure_logging_tests_test_cr_verifier_CPPFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CPPFLAGS)

modules_secure_logging_tests_test_cr_verifier_CFLAGS = \
	$(modules_secure_logging_tests_test_secure_logging_CFLAGS)

modules_secure_logging_tests_test_cr_verifier_LDADD = \
	$(modules_secure_logging_tests_test_secure_logging_LDADD)

endif

EXTRA_DIST += modules/secure-logging/tests/CMakeLists.txt
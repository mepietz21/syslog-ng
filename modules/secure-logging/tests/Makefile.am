# File: syslog-ng/modules/secure-logging/tests/Makefile.am

if ENABLE_SLOG

modules_secure_logging_tests_TESTS = \
	modules/secure-logging/tests/test_secure_logging \
	modules/secure-logging/tests/test_cr_common \
	modules/secure-logging/tests/test_cr_destination \
	modules/secure-logging/tests/test_cr_logger \
	modules/secure-logging/tests/test_cr_verifier

check_PROGRAMS += \
	${modules_secure_logging_tests_TESTS}

# modules_secure_logging_tests_test_secure_logging_CFLAGS	= \
#	$(TEST_CFLAGS) \
#	-I${top_srcdir}/modules/secure-logging \
#	-I${top_builddir}/modules/secure-logging

modules_secure_logging_tests_test_secure_logging_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_logger \
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
	$(modules_secure_logging_tests_test_secure_logging_LDADD)

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
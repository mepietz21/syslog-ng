# File: modules/secure-logging/crashrecovery/Makefile.am

if ENABLE_SLOG

bin_PROGRAMS += modules/secure-logging/crashrecovery/cr_tools/cr_log_count

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_SOURCES = \
	modules/secure-logging/crashrecovery/cr_tools/cr_log_count.c

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_LDADD = \
	modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
	$(GLIB_LIBS)

# -- C Preprocessor Flags (Include Paths) ---
modules_secure_logging_libsyslog_ng_secure_logging_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_logger \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_verifier

# -- Library Linkage ---
modules_secure_logging_libsyslog_ng_secure_logging_la_LIBADD += \
	modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
	modules/secure-logging/crashrecovery/cr_logger/libcr_logger.la \
	modules/secure-logging/crashrecovery/cr_verifier/libcr_verifier.la

# -- Sub-directory Includes ---
include modules/secure-logging/crashrecovery/cr_common/Makefile.am
include modules/secure-logging/crashrecovery/cr_logger/Makefile.am
include modules/secure-logging/crashrecovery/cr_verifier/Makefile.am
include modules/secure-logging/crashrecovery/cr_destination/Makefile.am

endif 

EXTRA_DIST += modules/secure-logging/crashrecovery/CMakeLists.txt

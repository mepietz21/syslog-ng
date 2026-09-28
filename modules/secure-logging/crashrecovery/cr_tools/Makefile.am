if ENABLE_SLOG

bin_PROGRAMS += modules/secure-logging/crashrecovery/cr_tools/cr_log_count

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_SOURCES = \
	modules/secure-logging/crashrecovery/cr_tools/cr_log_count.c

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_tools

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(OPENSSL_CFLAGS)

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_LDADD = \
	$(top_builddir)/modules/secure-logging/libutils_slog.la \
	$(top_builddir)/modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
	$(top_builddir)/lib/libsyslog-ng.la \
	$(GLIB_LIBS) \
	$(MODULE_DEPS_LIBS) \
	$(OPENSSL_LIBS) \
	$(LIBSYSLOG_SECURE_LOGGING) \
	-lm

endif

EXTRA_DIST += modules/secure-logging/crashrecovery/cr_tools/CMakeLists.txt
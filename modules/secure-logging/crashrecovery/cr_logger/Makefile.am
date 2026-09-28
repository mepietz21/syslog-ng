# File: modules/secure-logging/crashrecovery/cr_logger/Makefile.am

if ENABLE_SLOG

bin_PROGRAMS += modules/secure-logging/crashrecovery/cr_logger/cr_logger
bin_PROGRAMS += modules/secure-logging/crashrecovery/cr_tools/cr_log_count

# --- Binary: cr_logger ---
modules_secure_logging_crashrecovery_cr_logger_cr_logger_SOURCES = \
    modules/secure-logging/crashrecovery/cr_logger/cr_logger.c

modules_secure_logging_crashrecovery_cr_logger_cr_logger_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    -I$(top_srcdir)/modules/secure-logging \
    -I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common \
    -I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_logger

modules_secure_logging_crashrecovery_cr_logger_cr_logger_CFLAGS = \
    $(AM_CFLAGS) -O3 \
    $(GLIB_CFLAGS) \
    $(OPENSSL_CFLAGS)

modules_secure_logging_crashrecovery_cr_logger_cr_logger_LDADD = \
    $(top_builddir)/modules/secure-logging/crashrecovery/cr_logger/libcr_logger.la \
    $(top_builddir)/modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
    $(top_builddir)/modules/secure-logging/libutils_slog.la \
    $(top_builddir)/lib/libsyslog-ng.la \
    $(GLIB_LIBS) \
    $(MODULE_DEPS_LIBS) \
    $(OPENSSL_LIBS) \
    $(LIBSYSLOG_SECURE_LOGGING) \
    -lm


# --- Binary: cr_log_count ---
modules_secure_logging_crashrecovery_cr_tools_cr_log_count_SOURCES = \
    modules/secure-logging/crashrecovery/cr_tools/cr_log_count.c

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    -I$(top_srcdir)/modules/secure-logging \
    -I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_CFLAGS = \
    $(AM_CFLAGS) \
    $(GLIB_CFLAGS)

modules_secure_logging_crashrecovery_cr_tools_cr_log_count_LDADD = \
    $(top_builddir)/modules/secure-logging/crashrecovery/cr_common/libcr_common.la \
    $(top_builddir)/modules/secure-logging/libutils_slog.la \
    $(GLIB_LIBS)


# Static Library Target: libcr_logger.la

noinst_LTLIBRARIES += modules/secure-logging/crashrecovery/cr_logger/libcr_logger.la

modules_secure_logging_crashrecovery_cr_logger_libcr_logger_la_SOURCES = \
    modules/secure-logging/crashrecovery/cr_logger/cr_pi_logger.c \
    modules/secure-logging/crashrecovery/cr_logger/cr_pi_logger.h \
    modules/secure-logging/crashrecovery/cr_logger/cr_pi_logger_context.h

modules_secure_logging_crashrecovery_cr_logger_libcr_logger_la_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    -I$(top_srcdir)/modules/secure-logging \
    -I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common \
    -I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_logger

modules_secure_logging_crashrecovery_cr_logger_libcr_logger_la_CFLAGS = \
    $(AM_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(OPENSSL_CFLAGS)

endif

EXTRA_DIST += modules/secure-logging/crashrecovery/cr_logger/CMakeLists.txt
# File: modules/secure-logging/crashrecovery/cr_common/Makefile.am

if ENABLE_SLOG

noinst_LTLIBRARIES += modules/secure-logging/crashrecovery/cr_common/libcr_common.la

# -- Sources ---
modules_secure_logging_crashrecovery_cr_common_libcr_common_la_SOURCES = \
	modules/secure-logging/crashrecovery/cr_common/cr_crypto.c \
	modules/secure-logging/crashrecovery/cr_common/cr_pi_shared.c \
	modules/secure-logging/crashrecovery/cr_common/cr_randomstuff.c \
	modules/secure-logging/crashrecovery/cr_common/cr_crypto.h \
	modules/secure-logging/crashrecovery/cr_common/cr_pi_shared.h \
	modules/secure-logging/crashrecovery/cr_common/cr_randomstuff.h

# -- Target-Specific Preprocessor Flags (Safe from global pollution) ---
modules_secure_logging_crashrecovery_cr_common_libcr_common_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_srcdir)/modules/secure-logging/crashrecovery/cr_common

# -- Target-Specific Compiler Flags (Preserving global AM_CFLAGS) ---
modules_secure_logging_crashrecovery_cr_common_libcr_common_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(OPENSSL_CFLAGS)

# -- Linker Dependencies ---
modules_secure_logging_crashrecovery_cr_common_libcr_common_la_LIBADD = \
	$(GLIB_LIBS) \
	$(OPENSSL_LIBS) \
	-lm

endif

EXTRA_DIST += modules/secure-logging/crashrecovery/cr_common/CMakeLists.txt

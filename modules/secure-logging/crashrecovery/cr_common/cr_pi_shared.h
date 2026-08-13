/*
 * Copyright (c) 2019-2026 Airbus Commercial Aircraft
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */


#ifndef cr_pi_shared_h
#define cr_pi_shared_h

#include <stdint.h>
#include <glib.h>

/* logging */
#define CR_INFO_PREFIX "[SLOG_CR] INFO"
#define CR_WARNING_PREFIX "[SLOG_CR] WARNING"
#define CR_ERROR_PREFIX "[SLOG_CR] ERROR"

/* syslog-ng: 2048 octets must be supported at least, see RFC 5424. */
/* Note-1: Count of bytes is NOT equal to count of characters due to UTF-8 characters. */
/* One UTF-8 character might have 4 bytes */
/* Note-2: When a log string must be limited, program must take into account, */
/* that no invalid character is created at the limited log line! */
#define MESSAGE_LEN_SLOGCR 2048 /* The max len in bytes (octets) of an log entry message. */

#define KEY_SIZE 32

#define INTEGRITY_TAG_LEN 16 // The integrity tag len.
#define IV_SIZE 16
#define ID_LEN 16 // The ID (to find the correct key for this log entry) len.
#define MAC_LEN 16 // Encrypt then MAC HMAC len. ??CMAC??
#define CMAC_LEN 16
#define AES_BLOCK_LEN 16

#define CIPHERTEXT_LEN ((IV_SIZE) + (MESSAGE_LEN_SLOGCR) + (MAC_LEN))
#define LOG_LEN ((CIPHERTEXT_LEN) + (INTEGRITY_TAG_LEN) + (ID_LEN))

/* changed interface: explicite filename, up to caller now!  #define LOG_EXTENSION ".log.enc" */
#define KEY_EXTENSION ".key"

#define THE_K 5
#define THE_C 1.1244

/* crypto constants with a large hamming distance */
/* 4 constants of 1 byte, with a Hamming Distance of 4 */
#define PAD1 0xd
#define PAD2 0x3b
#define PAD3 0x51
#define PAD4 0xcb

/* Hash function FNV-1a constants */
#define FNV_PRIME_32 16777619
#define FNV_OFFSET_BASIS_32 2166136261U


/* --- Static Assert Abstraction: SLOGCR_STATIC_ASSERT --- */

/* Example how to use SLOGCR_STATIC_ASSERT */

/* Test 1: Passing assertion (should compile silently) */
/* SLOGCR_STATIC_ASSERT(1, "This should always pass"); */

/* Test 2: Failing assertion (compile-time error) */
/* SLOGCR_STATIC_ASSERT(0, "This should fail at compile time!"); */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
/* Modern C11 approach */
#include <assert.h>
#define SLOGCR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang extension for older standards */
#define SLOGCR_CONCAT_HELPER(a, b) a##b
#define SLOGCR_CONCAT(a, b) SLOGCR_CONCAT_HELPER(a, b)
#define SLOGCR_STATIC_ASSERT(cond, msg) \
    typedef char SLOGCR_CONCAT(slogcr_static_assertion_, __LINE__)[(cond) ? 1 : -1] __attribute__((unused))

#else
/* Basic C89/C99 fallback */
#define SLOGCR_CONCAT_HELPER(a, b) a##b
#define SLOGCR_CONCAT(a, b) SLOGCR_CONCAT_HELPER(a, b)
#define SLOGCR_STATIC_ASSERT(cond, msg) \
    typedef char SLOGCR_CONCAT(slogcr_static_assertion_, __LINE__)[(cond) ? 1 : -1]

#endif

/* --- End of Macro SLOGCR_STATIC_ASSERT --- */




/*
 * Function: CreateID
 * ------------------
 * ID_{l_j} = PRF_{K_i} (\gamma ' || j)
 */
int cr_CreateID(guchar *key, gint j, guchar IDlj[ID_LEN]);


/*
 * Function: CreateIntegrityTag
 * ----------------------------
 * T_{l_j} = PRF_{K_i} (XOR_{l_j})
 */
int cr_CreateIntegrityTag(guchar *key, guchar *XORlj, guchar Tlj[INTEGRITY_TAG_LEN]);

/* utility for logger and verifier to show GString str */
void cr_print_gstring_info(GString *gstr, const gchar *sz_title, gboolean is_showhex, gboolean is_debug);

/* get and check path form full filename */
gboolean get_path_from_file(const char *path_file_name, char *path_dir, size_t size_path_dir);

/* get short file name without extension */
gchar *get_stem_manually(const gchar *filename);


/* Timestamps, Diffs */

/* helper, get timestamp now as string (accuracity seconds) */
gboolean get_now(char *szNow, gsize str_size);

/* helper, timestamp milliseconds */
void diff_timespec(struct timespec t1, struct timespec t2, struct timespec *td);
int64_t timespec_as_milliseconds(struct timespec ts);
struct timespec get_ts_now(void);
int64_t get_time_diff_in_milliseconds(struct timespec ts0, struct timespec ts1, const char *szText);
GString *convert_time_diff(int64_t ndiff, const char *szText);
void get_minutes_seconds_from_ms(int64_t milliseconds, int *minutes, int *seconds);
void get_human_timestamp(char szBuffer[256]);
void get_filename_timestamp(char szBuffer[256]);

#endif /* cr_pi_shared_h */


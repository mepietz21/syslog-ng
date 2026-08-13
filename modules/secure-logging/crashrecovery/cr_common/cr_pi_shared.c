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


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // Required for uint64_t

#if defined(_XOPEN_SOURCE)
#include <time.h>
#endif

#include <glib.h>
#include "utils_slog.h"
#include "cr_pi_shared.h"
#include "cr_crypto.h"


// The folling must be replaced due to GitHub code checker
// static guchar GAMMA_DASH[AES_BLOCK_LEN] = {[0 ... (AES_BLOCK_LEN - 1)] = PAD2};
// cr_pi_shared.h:54: #define AES_BLOCK_LEN 16

SLOGCR_STATIC_ASSERT(AES_BLOCK_LEN == 16, "Wrong_AES_Block_Size_for_provided_GAMMA_DASH_initialization");

#define FILLGD_16(val) val, val, val, val, val, val, val, val, \
                      val, val, val, val, val, val, val, val

static guchar GAMMA_DASH[AES_BLOCK_LEN] = { FILLGD_16(PAD2) };


//-- from PIShared.c -----

int cr_CreateID(guchar *key, gint j, guchar IDlj[ID_LEN])
{
  gint inputSize = ID_LEN + sizeof(int);
  guchar inputBuffer[inputSize];

  memcpy(inputBuffer, GAMMA_DASH, AES_BLOCK_LEN);
  memcpy(inputBuffer + AES_BLOCK_LEN, &j, sizeof(int));

  if (cr_PRF(inputBuffer, inputSize, key, IDlj, ID_LEN) == 0)
    {
      // fixed: msg_error
      msg_error("Failed to create ID");
      return 0;
    }

  return 1;
}

int cr_CreateIntegrityTag(guchar *key, guchar *XORlj, guchar Tlj[INTEGRITY_TAG_LEN])
{
  if (0 == cr_PRF(XORlj, CIPHERTEXT_LEN, key, Tlj, INTEGRITY_TAG_LEN))
    {
      // fixed: msg_error
      msg_error("Failed to create the integrity tag.");
      return 0;
    }
  return 1;
}


//----------------------------------------------------------------------
// cr_print_gstring_info
//
// Show character and octet count, the string itself and the bytes in hex
//
// in gstr: GString instance that info shall be printed on terminal
// in sz_title: variable name of GString instance or info
// in is_showhex: Show bytes as hex numbers
// in is_debug: print to terminal at all
//
// return -

void cr_print_gstring_info(GString *gstr, const gchar *sz_title, gboolean is_showhex, gboolean is_debug)
{
  if (FALSE == is_debug)
    return;
  if (NULL == gstr)
    return;
  if (NULL == sz_title)
    return;
  glong cnt_of_symbols = g_utf8_strlen(gstr->str, -1);
  g_print("symbols: %ld, octets: %ld\n%s\n", cnt_of_symbols, gstr->len, gstr->str);
  if (TRUE == is_showhex && gstr->len > 0)
    {
      // TODO autotools makefiles
      //dbg_hexdump(sz_title, (guchar *) gstr->str, gstr->len);
      ;
    }
}


// return TRUE on success, else FALSE
gboolean get_path_from_file(const char *path_file_name, char *path_dir, size_t size_path_dir)
{
  gboolean retval = FALSE;
  if ((NULL == path_file_name) || (NULL == path_dir))
    {
      // fixed: msg_warning
      msg_warning("Invalid input, get_path_from_file");
      return FALSE; //-- ERROR, never ever
    }
  memset(path_dir, 0, size_path_dir);
  gchar *dirname = g_path_get_dirname(path_file_name);
  if (NULL != dirname)
    {
      if (g_file_test(dirname, G_FILE_TEST_IS_DIR))
        {
          size_t dir_len = strlen(dirname);

          // Check if the directory name fits (including the null terminator)
          if (dir_len >= size_path_dir)
            {
              g_free(dirname);
              // fixed: msg_warning
              msg_warning("size_path_dir too small",
                          evt_tag_printf("size_path_dir", "%zu", size_path_dir),
                          evt_tag_printf("required_size", "%zu", dir_len + 1));
              return FALSE;
            }

          // Since you memset path_dir to 0, you can use strncpy or g_strlcpy
          g_strlcpy(path_dir, dirname, size_path_dir);
          retval = TRUE;
        }
    }
  g_free (dirname);
  return retval;
}



//----------------------------------------------------------------------
// get_stem_manually
//
// Extracts and creates a name based on a file name by removing extension
//
// returns string, caller owns

gchar *get_stem_manually(const gchar *filename)
{
  //-- Find the last occurrence of '.' in the string
  gchar *dot = g_strrstr(filename, ".");

  //-- If no dot is found, or if it's the first character (dotfile),
  //   then just duplicate the whole string.
  if (dot == NULL || dot == filename)
    {
      return g_strdup(filename);
    }

  //-- Otherwise, create a new string from the beginning up to the dot
  return g_strndup(filename, dot - filename);
}



//----------------------------------------------------------------------
// get_now
//
// Get current timestamp as szString
// in/out szNow: String buffer that will contain current time in format "%Y-%m-%d_%H%M%S"
// in str_size: Size of string buffer szNow
// returns TRUE in case of success else FALSE

gboolean get_now(char *szNow, gsize str_size)
{
  memset(szNow, 0, str_size);//    char filename_buffer[64];
  const struct tm *local_time_info;
  time_t raw_time;
  time(&raw_time);
  local_time_info = localtime(&raw_time);
  if (strftime(szNow, str_size, "%Y-%m-%d_%H%M%S", local_time_info) == 0)
    {
      // fixed: msg_warning
      msg_warning("Failed to format the time string.");
      return FALSE; //-- ERROR
    }
  return TRUE; //-- SUCCESS
}



//----------------------------------------------------------------------
// get_timestamp_diff
//
// Get time difference in scondes of two timestamps provided as string
// in szStart: String buffer that will contain start time in format "%Y-%m-%d_%H%M%S"
// in szEnd: String buffer that will contain end time in format "%Y-%m-%d_%H%M%S"
// in/out szDiff: string buffer of time difference
// in size: Size of diff string buffer
// returns TRUE in case of success else FALSE

gboolean get_timestamp_diff(const char *szStart, const char *szEnd, char *szDiff, gsize str_size)
{
  const char *time_format = "%Y-%m-%d_%H%M-%S";
  struct tm time_info1;
  struct tm time_info2;
  memset(&time_info1, 0, sizeof(struct tm));
  memset(&time_info2, 0, sizeof(struct tm));

  // --- Step 1: Parse the strings into struct tm ---
  // strptime returns a pointer to the first character it couldn't parse.
  // On success, this should be the null terminator at the end of the string.
  if (strptime(szStart, time_format, &time_info1) == NULL)
    {
      // fixed: msg_warning
      msg_warning("Could not parse the first timestamp",
                evt_tag_str("timestamp", szStart));
      return FALSE; //-- ERROR
    }

  if (strptime(szEnd, time_format, &time_info2) == NULL)
    {
      // fixed: msg_warning
      msg_warning("Could not parse the second timestamp",
                evt_tag_str("timestamp", szEnd));
      return FALSE; //-- ERROR
    }

  // --- Step 2: Convert struct tm to time_t ---
  // mktime converts a local time structure into a calendar time representation
  time_t t1 = mktime(&time_info1);
  time_t t2 = mktime(&time_info2);
  if (t1 == -1 || t2 == -1)
    {
      // fixed: msg_warning
      msg_warning("Failed to convert timestamp to calendar time");
      return FALSE; //-- ERROR
    }

  // --- Step 3: Calculate the difference ---
  // difftime returns the difference in seconds as a double
  double diff_in_seconds = difftime(t2, t1);
  g_snprintf(szDiff, str_size, "%.f seconds", diff_in_seconds);

  return TRUE; //-- SUCCESS
}



//----------------------------------------------------------------------
// diff_timespec
//
// in t1: timestamp start
// in t2: timestamp end
// in/out td: timestamp difference
//
// based on
// https://stackoverflow.com/questions/74402102/in-struct-timespec-tv-sec-and-tv-nsec-express-the-same-time-in-seconds-and-in-na
// define NS_PER_SECOND 1000000000
// returns -

void diff_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
  long NS_PER_SECOND = 1000000000L;
  td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
  td->tv_sec  = t2.tv_sec - t1.tv_sec;
  if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
      td->tv_nsec += NS_PER_SECOND;
      td->tv_sec--;
    }
  else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
      td->tv_nsec -= NS_PER_SECOND;
      td->tv_sec++;
    }
}



//----------------------------------------------------------------------
// timespec_as_milliesconds
//
// Get time stamp as integer value
// in ts: struct provided by clock_gettime. Used by function get_ts_now().
// returns integer value from ts in unit milliseconds

int64_t timespec_as_milliseconds(struct timespec ts)
{
  int64_t rv = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  return rv;
}



//----------------------------------------------------------------------
// get_ts_now
//
// Get time stamp of now as struct timespec
// returns struct timespec

struct timespec get_ts_now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts); //-- best for diff
  return ts;
}



//----------------------------------------------------------------------
// get_time_diff_in_millisecondes
//
// Get time stamp diff in milliseconds. The caller is expected to use
// struct timespec ts1 = get_ts_now();
// to get ts0 and ts1 timespec.
// Also the converted values for minutes and seconds are printed.
//
// in ts0: timestamp start by get_ts_now()
// in ts1: timestamp end by get_ts_now()
// in szText: When not NULL, used to print on termnial togehter with
//            milliseconds value
//            szText can be set explicitly to NULL in order to not print
//            on terminal
//
// returns milliseonds value

int64_t get_time_diff_in_milliseconds(struct timespec ts0, struct timespec ts1, const char *szText)
{
  struct timespec ts_diff;
  diff_timespec(ts0, ts1, &ts_diff);
  int64_t ndiff = timespec_as_milliseconds(ts_diff);
  if (NULL == szText)
    {
      //-- the caller does not want us to log on terminal
      return ndiff;
    }
  g_print("%s: %ld milliseconds\n", szText, ndiff);
  if (ndiff > 500)
    {
      int minutes = 0;
      int seconds = 0;
      get_minutes_seconds_from_ms(ndiff, &minutes, &seconds);
      g_print("%d minutes and %d seconds\n", minutes, seconds);
    }
  return ndiff;
}



//----------------------------------------------------------------------
// convert_time_diff
//
// Converts given milliseconds value into a GString which will also
// contain the value for minutes and seconds and a caller specified
// text.
// The caller has to free the memory when the Gstring is not needed anymore.
// GString *gs = convert_time_diff(...
// ...
// g_string_free(gs, TRUE);
// gs = NULL;
//
// in ndiff: number of milliseconds
// in szText: when not NULL, text from caller to be part of returned
//            GString. This will can be set explicit to NULL.
//            milliseconds value
//
// returns GString pointer. The caller owns the memory.

GString *convert_time_diff( int64_t ndiff, const char *szText)
{
  GString *gstr = NULL;
  gstr = g_string_new("");
  if (NULL != szText)
    {
      g_string_append_printf(gstr, "%s, ", szText);
    }
  int minutes = 0;
  int seconds = 0;
  get_minutes_seconds_from_ms(ndiff, &minutes, &seconds);
  g_string_append_printf(gstr, "%ld milliseconds (== %d minutes and %d seconds)\n",
                         ndiff, minutes, seconds);
  return gstr;
}



//----------------------------------------------------------------------
// get_minuts_seconds_from_ms
//
// Converts a millisecond value into minutes and seconds.
//
// in milliseconds The total duration in milliseconds.
// in/out minutes  A pointer to an integer to store the calculated minutes.
// in/out seconds  A pointer to an integer to store the calculated seconds.
//
// return -

void get_minutes_seconds_from_ms(int64_t milliseconds, int *minutes, int *seconds)
{
  if ((NULL == minutes) || (NULL == seconds))
    {
      return;
    }
  uint64_t total_seconds = (milliseconds + 500) / 1000;
  *minutes = (int) (total_seconds / 60);
  *seconds = (int) (total_seconds % 60);
}



//----------------------------------------------------------------------
// get_human_timestamp
//
// Fills given buffer with current timestamp
//
// in/out szBuffer  Buffer provided by caller to keep timestamp
//
// return -

void get_human_timestamp(char szBuffer[256])
{
  struct timespec current_ts;
  clock_gettime(CLOCK_REALTIME, &current_ts);
  time_t seconds = current_ts.tv_sec;
  const struct tm *local_time = localtime(&seconds);
  strftime(szBuffer, 256, "%Y-%m-%d %H:%M:%S", local_time);
}



//----------------------------------------------------------------------
// get_filename_timestamp
//
// Fills given buffer with current timestamp which is suitable for a
// filename part
//
// in/out szBuffer  Buffer provided by caller to keep timestamp
//
// return -

void get_filename_timestamp(char szBuffer[256])
{
  struct timespec current_ts;
  clock_gettime(CLOCK_REALTIME, &current_ts);
  time_t seconds = current_ts.tv_sec;
  const struct tm *local_time = localtime(&seconds);
  strftime(szBuffer, 256, "%Y-%m-%dT%H%M%S", local_time);
}


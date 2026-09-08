/*
* Copyright (c) 2026 Airbus Commercial Aircraft
* Unit Tests for syslog-ng secure-logging crash recovery logger (cr_pi_logger)
* Framework: Criterion
*/
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "cr_pi_shared.h"
#include "cr_crypto.h"
#include "cr_pi_logger_context.h"
#include "cr_pi_logger.h"
/* Helper function to generate a dummy master key file */
static void create_dummy_key_file(const char *path)
{
guchar dummy_key[KEY_SIZE];
memset(dummy_key, 0x42, KEY_SIZE);
gboolean res = cr_write_key(path, dummy_key);
cr_assert_eq(res, TRUE, "Failed to create dummy master key file");
}
/*
======================================================================
===
* TEST SUITE: Key Read/Write Operations
*
======================================================================
=== */
Test(cr_logger_keys, read_write_key_roundtrip)
{
gchar *temp_key_path = g_build_filename(g_get_tmp_dir(), "test_cr_rw.key", NULL);
guchar write_key[KEY_SIZE];
guchar read_key[KEY_SIZE];
memset(write_key, 0xAB, KEY_SIZE);
memset(read_key, 0x00, KEY_SIZE);
/* Test Writing */
gboolean write_res = cr_write_key(temp_key_path, write_key);
cr_assert_eq(write_res, TRUE, "cr_write_key failed");

/* Test Reading */
gboolean read_res = cr_read_key(temp_key_path, read_key);
cr_assert_eq(read_res, TRUE, "cr_read_key failed");
/* Verify Match */
cr_assert_arr_eq(read_key, write_key, KEY_SIZE, "Read key does not match written key");
g_unlink(temp_key_path);
g_free(temp_key_path);
}
Test(cr_logger_keys, read_nonexistent_key)
{
guchar read_key[KEY_SIZE];
gboolean res = cr_read_key("/nonexistent_path/invalid_key.key", read_key);
cr_assert_eq(res, FALSE, "cr_read_key should fail on nonexistent file");
}
/*
======================================================================
===
* TEST SUITE: Encryption Logic
*
======================================================================
=== */
Test(cr_logger_encrypt, encrypt_log_message_length)
{
guchar key[KEY_SIZE] = "01234567890123456789012345678901";
const unsigned char *msg = (const unsigned char *)"Test syslog message line";
size_t msg_len = strlen((const char *)msg);
unsigned char cipherLogMessage[IV_SIZE + MESSAGE_LEN_SLOGCR + MAC_LEN];
memset(cipherLogMessage, 0, sizeof(cipherLogMessage));
int cipher_len = cr_encryptLog(key, msg, msg_len, cipherLogMessage);
int expected_len = IV_SIZE + MESSAGE_LEN_SLOGCR + MAC_LEN;
cr_assert_eq(cipher_len, expected_len, "cr_encryptLog returned length %d, expected %d", cipher_len, expected_len);

}
/*
======================================================================
===
* TEST SUITE: Context & Full Logger Lifecycle

*
======================================================================
=== */
Test(cr_logger_lifecycle, pi_context_creation_and_add_log)
{
GError *error = NULL;
gchar *tmp_dir = g_dir_make_tmp("test_slog_cr_XXXXXX", &error);
cr_assert_not_null(tmp_dir, "Failed to create temporary directory");
gchar *master_key_path = g_build_filename(tmp_dir, "master.key", NULL);
gchar *enc_log_path = g_build_filename(tmp_dir, "test_output.enc", NULL);
/* 1. Create initial master key */
create_dummy_key_file(master_key_path);
/* 2. Setup Logger Context (maxLogs = 6, which is > THE_K = 5) */
cr_pi_logger_context loggerCtx;
loggerCtx.p_MasterKeyPath = master_key_path;
loggerCtx.p_OutputDirectoryPath = tmp_dir;
loggerCtx.p_InputPlainLogPath = NULL;
loggerCtx.p_OutputEncLogPath = enc_log_path;
loggerCtx.maxLogs = 6;
cr_PIContext *ctx = NULL;
cr_PRGContext *prg = NULL;
/* 3. Initialize Logger */
gboolean init_res = init_cr_logger_functionality(&loggerCtx, &ctx, &prg);
cr_assert_eq(init_res, TRUE, "init_cr_logger_functionality failed");
cr_assert_not_null(ctx, "ctx should be initialized");
/* 4. Add Log Entries */
const unsigned char *log1 = (const unsigned char *)"Test Log Line 1";
gboolean add_res1 = cr_AddLogEntry(ctx, log1, strlen((const char *)log1));
cr_assert_eq(add_res1, TRUE, "cr_AddLogEntry failed on first log");
const unsigned char *log2 = (const unsigned char *)"Test Log Line 2";
gboolean add_res2 = cr_AddLogEntry(ctx, log2, strlen((const char *)log2));
cr_assert_eq(add_res2, TRUE, "cr_AddLogEntry failed on second log");
/* 5. Clean up */
if (ctx->logFile) fclose(ctx->logFile);
if (ctx->keyFile) fclose(ctx->keyFile);
g_free(ctx->keyPath);
g_free(ctx);
g_free(prg);

g_unlink(master_key_path);
g_unlink(enc_log_path);
g_rmdir(tmp_dir);
g_free(master_key_path);
g_free(enc_log_path);
g_free(tmp_dir);
}
/*
======================================================================
===
* TEST SUITE: Log File Reading & UTF-8 Sanitization
*
======================================================================
=== */
Test(cr_logger_reader, read_and_sanitize_logs)
{
GError *error = NULL;
gchar *tmp_file_path = g_build_filename(g_get_tmp_dir(), "test_plain_input.txt", NULL);
/* Prepare a file containing 1 valid UTF-8 line and 1 line with an invalid byte 0xFF */
FILE *f = fopen(tmp_file_path, "wb");
cr_assert_not_null(f, "Failed to create input log file");
fputs("Valid UTF-8 Line\n", f);
fputs("Line with invalid \xFF byte\n", f);
fclose(f);
/* Read up to 10 logs using GLib channel reader */
GPtrArray *logs = cr_pi_logger_main_read_logs_glib(tmp_file_path, 10, &error);
cr_assert_null(error, "Error set during log reading: %s", error ? error->message : "");
cr_assert_not_null(logs, "g_ptr_array should not be NULL");
cr_assert_eq(logs->len, 2, "Expected 2 lines read, got %u", logs->len);
/* Verify Line 1 (Valid) */
GString *line1 = (GString *)g_ptr_array_index(logs, 0);
cr_assert_str_eq(line1->str, "Valid UTF-8 Line\n");
/* Verify Line 2 (Invalid byte 0xFF replaced by hex string "FF") */
GString *line2 = (GString *)g_ptr_array_index(logs, 1);
cr_assert_not_null(strstr(line2->str, "FF"), "Invalid byte 0xFF was not converted to hex'FF'");
/* Cleanup */
g_ptr_array_free(logs, TRUE);
g_unlink(tmp_file_path);

g_free(tmp_file_path);
}
/*
 * File: cr_log_count.c
 * Standalone tool to inspect encrypted Crash Recovery log files and output capacity (n).
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "cr_pi_shared.h"
#include "messages.h"
#include "utils_slog.h"

int main(int argc, char *argv[])
{
  gchar *enc_log_path = NULL;
  GError *error = NULL;

  GOptionEntry entries[] =
  {
    { "in", 'i', 0, G_OPTION_ARG_FILENAME, &enc_log_path, "Path to encrypted log file", "FILE" },
    { 0 }
  };

  GOptionContext *context = g_option_context_new("- Crash Recovery Log Counter");
  g_option_context_add_main_entries(context, entries, NULL);

  if (!g_option_context_parse(context, &argc, &argv, &error))
    {
      g_printerr("ERROR: Failed to parse arguments: %s\n", error->message);
      g_error_free(error);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if (NULL == enc_log_path && argc > 1)
    {
      /* Fallback: First positional argument */
      enc_log_path = g_strdup(argv[1]);
    }

  if (NULL == enc_log_path)
    {
      g_printerr("Usage: %s -i <encrypted_log_file>\n", argv[0]);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  gsize count = 0;

  gboolean success = get_plain_log_lines_from_cr_logger_enc(enc_log_path, LOG_LEN, THE_C, &count);

  if (!success)
  {
      g_printerr("ERROR: Could not read capacity from %s\n", enc_log_path);
      g_free(enc_log_path);
      return EXIT_FAILURE;
  }

  g_print("Encrypted file: %s\n", enc_log_path);
  g_print("Estimated Log Capacity (n): %zu lines\n", count);

  g_option_context_free(context);

  if (count < 0)
    {
      g_printerr("ERROR: Could not read capacity from %s\n", enc_log_path);
      g_free(enc_log_path);
      return EXIT_FAILURE;
    }

  g_print("Encrypted file: %s\n", enc_log_path);
  g_print("Estimated Log Capacity (n): %ld lines\n", count);

  g_free(enc_log_path);
  return EXIT_SUCCESS;
}
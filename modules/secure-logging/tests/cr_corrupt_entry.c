/*
 * File: cr_corrupt_entry.c
 * Standalone tool to simulate a crash-corrupted entry in a CR encrypted log file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "cr_pi_shared.h"
#include "messages.h"

int main(int argc, char *argv[])
{
  gchar *enc_log_path = NULL;
  gint entry_index = -1;
  GError *error = NULL;

  GOptionEntry entries[] =
  {
    { "in", 'i', 0, G_OPTION_ARG_FILENAME, &enc_log_path,
      "Path to encrypted log file", "FILE" },
    { "entry", 'e', 0, G_OPTION_ARG_INT, &entry_index,
      "Zero-based index of entry to corrupt", "INDEX" },
    { 0 }
  };

  GOptionContext *context =
    g_option_context_new("- Corrupt complete entry in CR encrypted log");

  g_option_context_add_main_entries(context, entries, NULL);

  if (!g_option_context_parse(context, &argc, &argv, &error))
    {
      g_printerr("ERROR: Failed to parse arguments: %s\n", error->message);
      g_error_free(error);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if ((NULL == enc_log_path) || (entry_index < 0))
    {
      g_printerr("Usage: %s -i <encrypted_log_file> -e <entry_index>\n",
                 argv[0]);
      g_option_context_free(context);
      g_free(enc_log_path);
      return EXIT_FAILURE;
    }

  FILE *f = fopen(enc_log_path, "r+b");
  if (NULL == f)
    {
      g_printerr("ERROR: Could not open file %s\n", enc_log_path);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if (fseek(f, 0, SEEK_END) != 0)
    {
      g_printerr("ERROR: Could not seek to end of file\n");
      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  long file_size = ftell(f);
  if (file_size < 0)
    {
      g_printerr("ERROR: Could not determine file size\n");
      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if ((file_size <= 0) || ((file_size % LOG_LEN) != 0))
    {
      g_printerr(
        "ERROR: Invalid file size %ld "
        "(must be non-zero multiple of LOG_LEN=%d)\n",
        file_size, LOG_LEN);

      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  long total_entries = file_size / LOG_LEN;

  if (entry_index >= total_entries)
    {
      g_printerr(
        "ERROR: Entry index %d out of bounds "
        "(total entries: %ld)\n",
        entry_index, total_entries);

      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  /*
   * Overwrite exactly one complete entry with zero bytes.
   *
   * This intentionally does not shift subsequent entries and does not
   * change the file size. The file geometry therefore remains unchanged.
   */
  unsigned char zero_buffer[LOG_LEN];
  memset(zero_buffer, 0, sizeof(zero_buffer));

  long offset = (long)entry_index * LOG_LEN;

  if (fseek(f, offset, SEEK_SET) != 0)
    {
      g_printerr(
        "ERROR: Could not seek to entry %d at offset %ld\n",
        entry_index,
        offset);

      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if (fwrite(zero_buffer, 1, LOG_LEN, f) != LOG_LEN)
    {
      g_printerr(
        "ERROR: Could not corrupt entry %d\n",
        entry_index);

      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if (fflush(f) != 0)
    {
      g_printerr("ERROR: Failed to flush modified log\n");
      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  fclose(f);

  g_print(
    "Successfully corrupted entry %d in %s "
    "(offset: %ld, size unchanged: %ld bytes, %ld entries)\n",
    entry_index,
    enc_log_path,
    offset,
    file_size,
    total_entries);

  g_free(enc_log_path);
  g_option_context_free(context);

  return EXIT_SUCCESS;
}
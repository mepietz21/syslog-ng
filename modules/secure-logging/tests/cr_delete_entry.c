/*
 * File: cr_delete_entry.c
 * Standalone tool to physically delete or zero-out an entry from a CR encrypted log file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>

#include "cr_pi_shared.h"

int main(int argc, char *argv[])
{
  gchar *enc_log_path = NULL;
  gint entry_index = -1;
  gboolean zero_mode = FALSE;
  GError *error = NULL;

  GOptionEntry entries[] =
  {
    { "in", 'i', 0, G_OPTION_ARG_FILENAME, &enc_log_path,
      "Path to encrypted log file", "FILE" },
    { "entry", 'e', 0, G_OPTION_ARG_INT, &entry_index,
      "Zero-based index of entry to process", "INDEX" },
    { "zero", 'z', 0, G_OPTION_ARG_NONE, &zero_mode,
      "Zero-out entry in-place instead of shifting/truncating", NULL },
    { 0 }
  };

  GOptionContext *context =
    g_option_context_new("- Delete or zero-out entry from CR encrypted log");
  g_option_context_add_main_entries(context, entries, NULL);

  if (!g_option_context_parse(context, &argc, &argv, &error))
    {
      g_printerr("ERROR: Failed to parse arguments: %s\n", error->message);
      g_error_free(error);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if (NULL == enc_log_path || entry_index < 0)
    {
      g_printerr("Usage: %s -i <encrypted_log_file> -e <entry_index> [-z]\n",
                 argv[0]);
      g_option_context_free(context);
      g_free(enc_log_path);
      return EXIT_FAILURE;
    }

  FILE *f = fopen(enc_log_path, "r+b");
  if (!f)
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
  if (file_size <= 0 || (file_size % LOG_LEN) != 0)
    {
      g_printerr("ERROR: Invalid file size %ld (must be non-zero multiple of LOG_LEN=%d)\n",
                 file_size, LOG_LEN);
      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  long total_entries = file_size / LOG_LEN;

  if (entry_index >= total_entries)
    {
      g_printerr("ERROR: Entry index %d out of bounds (total entries: %ld)\n",
                 entry_index, total_entries);
      fclose(f);
      g_free(enc_log_path);
      g_option_context_free(context);
      return EXIT_FAILURE;
    }

  if (zero_mode)
    {
      /* In-Place Zeroing (-z) */
      unsigned char zero_buffer[LOG_LEN];
      memset(zero_buffer, 0, LOG_LEN);

      if (fseek(f, entry_index * LOG_LEN, SEEK_SET) != 0)
        {
          g_printerr("ERROR: Seek error at block %d\n", entry_index);
          fclose(f);
          g_free(enc_log_path);
          g_option_context_free(context);
          return EXIT_FAILURE;
        }

      if (fwrite(zero_buffer, 1, LOG_LEN, f) != LOG_LEN)
        {
          g_printerr("ERROR: Write error at block %d\n", entry_index);
          fclose(f);
          g_free(enc_log_path);
          g_option_context_free(context);
          return EXIT_FAILURE;
        }

      fclose(f);
      g_print("Successfully zeroed entry %d in %s\n", entry_index, enc_log_path);
    }
  else
    {
      /* Shift & Truncate */
      unsigned char buffer[LOG_LEN];

      for (long i = entry_index + 1; i < total_entries; i++)
        {
          if (fseek(f, i * LOG_LEN, SEEK_SET) != 0 ||
              fread(buffer, 1, LOG_LEN, f) != LOG_LEN ||
              fseek(f, (i - 1) * LOG_LEN, SEEK_SET) != 0 ||
              fwrite(buffer, 1, LOG_LEN, f) != LOG_LEN)
            {
              g_printerr("ERROR: Read/Write error during shift\n");
              fclose(f);
              g_free(enc_log_path);
              g_option_context_free(context);
              return EXIT_FAILURE;
            }
        }

      int fd = fileno(f);
      long new_file_size = file_size - LOG_LEN;
      if (ftruncate(fd, new_file_size) != 0)
        {
          g_printerr("ERROR: Failed to truncate file\n");
          fclose(f);
          g_free(enc_log_path);
          g_option_context_free(context);
          return EXIT_FAILURE;
        }

      fclose(f);
      g_print("Successfully deleted entry %d from %s (new size: %ld bytes)\n",
              entry_index, enc_log_path, new_file_size);
    }

  g_free(enc_log_path);
  g_option_context_free(context);
  return EXIT_SUCCESS;
}
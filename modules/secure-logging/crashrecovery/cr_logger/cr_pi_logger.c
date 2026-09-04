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


#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "utils_slog.h"
#include "cr_pi_logger.h"
#include "cr_crypto.h"


#define SESSION_KEY

//----------------------------------------------------------------------
// cr_AddLogEntry
//
// Encrypts plain text log entry.
// When this function returns 0, the caller is expected to terminate.
//
// in/out ctx:
// in/out logMessage:
// in logMessageSize:
//
// returns TRUE on SUCCESS and FALSE on ERROR

gboolean cr_AddLogEntry(cr_PIContext *ctx, const unsigned char *logMessage, size_t logMessageSize)
{
  unsigned char encKey[KEY_SIZE], drnKey[KEY_SIZE], tagKey[KEY_SIZE], idKey[KEY_SIZE],
           cipherLogMessage[MESSAGE_LEN_SLOGCR + IV_SIZE + MAC_LEN],
           TauiBuffer[LOG_LEN],
           XORlj[MESSAGE_LEN_SLOGCR + IV_SIZE + MAC_LEN],
           Tlj[INTEGRITY_TAG_LEN],
           IDlj[ID_LEN];

  int kRandom[THE_K];

  if (NULL == ctx)
    {
      g_warning("cr_AddLogEntry: Invalid cr_PIContext pointer ctx!\n");
      return FALSE; //-- ERROR
    }

  if (NULL == logMessage)
    {
      g_warning("cr_AddLogEntry: Invalid logMessage pointer!\n");
      return FALSE; //-- ERROR
    }

  // derive keys
  if (0 == cr_DeriveSubKeys(ctx->sessionKey, encKey, drnKey, tagKey, idKey))
    {
      g_warning("Failed to derive sub.\n"); //-- caller terminates
      return FALSE; //-- ERROR
    }

  // cipherLogMessage = malloc(MESSAGE_LEN_SLOGCR + IV_SIZE + MAC_LEN);
  // line 1
  if (0 == cr_encryptLog(encKey, logMessage, logMessageSize, cipherLogMessage))
    {
      g_warning("Failed to encrypt the log message.\n"); //-- caller terminates
      return FALSE;
    }

  // Create the k distinct random locations within the number m (which is the log file "length").
  // line 2
  //g_print("cr_DRN, line 2\n");
  if (0 == cr_DRN(drnKey, THE_K, ctx->m, kRandom))
    {
      g_warning("Failed to create k distinct random numbers.\n"); //-- caller terminates
      return FALSE;
    }

  // XOR ci (the encrypted log file) at the k distinct random locations within the log file.
  // line 4
  int l;
  for (int j = 0; j < THE_K; ++j)
    {
      l = kRandom[j];

      // seek to the requiered location within the log file
      if (fseek(ctx->logFile, l * LOG_LEN, SEEK_SET) != 0)
        {
          g_warning("Unable to move the file position indicator.\n");
          return FALSE;
        }

      // Check for read errors or end-of-file
      if (ferror(ctx->logFile))
        {
          int errnum = errno;
          g_warning("Error opening file: %s\n", strerror(errnum));
          return FALSE;
        }

      // read the location from the log file.
      if (fread(TauiBuffer, 1, LOG_LEN, ctx->logFile) != LOG_LEN)
        {
          int errnum = errno;
          g_warning("Error opening file: %s\n", strerror(errnum));
          return FALSE;
        }

      // cipher ⊕ XORlj
      // XOR the XOR parts.
      // line 5
      for (int i = 0; i < CIPHERTEXT_LEN; ++i)
        {
          XORlj[i] = TauiBuffer[i] ^ cipherLogMessage[i];
        }

      // create integrity TAG
      // line 6
      if (0 == cr_CreateIntegrityTag(tagKey, XORlj, Tlj))
        {
          g_warning("Failed to create the integrity tag.\n");
          return FALSE;
        }

      // create ID
      // line 7
      if (0 == cr_CreateID(idKey, j, IDlj))
        {
          g_warning("Failed to create ID.\n");
          return FALSE;
        }

      // seek to the position again
      if (fseek(ctx->logFile, l * LOG_LEN, SEEK_SET) != 0)
        {
          g_warning("Unable to move the file position indicator.\n");
          return FALSE;
        }

      memcpy(TauiBuffer, XORlj, CIPHERTEXT_LEN);
      memcpy(TauiBuffer + CIPHERTEXT_LEN, Tlj, INTEGRITY_TAG_LEN);
      memcpy(TauiBuffer + CIPHERTEXT_LEN + INTEGRITY_TAG_LEN, IDlj, ID_LEN);

      // write back to file
      if (fwrite(TauiBuffer, 1, LOG_LEN, ctx->logFile) != LOG_LEN)
        {
          g_warning("Failed to write the XORed log message back.\n");
          return FALSE;
        }

    } //-- for line 4

  // key evolution
  // line 9
  if (FALSE == cr_updateKey(ctx) )
    {
      g_warning("Failed: update Key!\n");
      return FALSE; //-- ERROR
    }

  return TRUE; //-- SUCCESS
}



//----------------------------------------------------------------------
// cr_Init
//
// Initializes random generator. When this function returns FALSE,
// the caller is expected to terminate the program.
//
// in ctx: cr_PIContext
//
// returns TRUE on SUCCESS else FALSE
gboolean cr_Init(cr_PIContext *ctx)
{
  if (NULL == ctx)
    {
      g_warning("Invalid ctx pointer!");
      return FALSE; //-- ERROR
    }
  if (NULL == ctx->logFile)
    {
      g_warning("Invalid ctx->logFile pointer!");
      return FALSE; //-- ERROR
    }
  if (NULL == ctx->keyFile)
    {
      g_warning("Invalid ctx->keyFile pointer!");
      return FALSE; //-- ERROR
    }

  size_t fileSize = (size_t) ctx->m * LOG_LEN; // m * LOG_LEN
  g_print("cr_Init, fileSize: %ld, ctx->m: %d, LOG_LEN: %d\n", fileSize, ctx->m, LOG_LEN);

  // line 2
  if (cr_createNewLogFile(ctx->logFile, fileSize) == FALSE)
    {
      g_warning("Failed to create new log file!");
      return FALSE; //-- ERROR
    }

  // declare context outside of the file preperation, to reuse the given context if more then one file will be filled up. (not in this thesis)
  cr_PRGContext *prgContext = cr_CreatePRGContext(ctx->sessionKey);
  if (NULL == prgContext)
    {
      g_warning("Failed to create the PRGContext.");
      return FALSE; //-- ERROR
    }

  // line 4
  // write random pad.
  gboolean ret_randompad = cr_initializeLogFileWithPseudoRandomPad(prgContext, ctx->logFile, ctx->m);
  g_free(prgContext);
  if (FALSE == ret_randompad)
    {
      g_warning("cr_initalizedLogFileWithPseudoRandomPad returns with error!");
      return FALSE; //-- ERROR
    }

  // First key evolution.
  // line 6
  cr_updateKey(ctx);
  return TRUE; //-- SUCCESS
}



gboolean cr_Init_prg(cr_PIContext *ctx, cr_PRGContext **pp_prg)
{
  if (NULL == ctx)
    {
      g_warning("Invalid ctx pointer!");
      return FALSE; //-- ERROR
    }
  if (NULL == ctx->logFile)
    {
      g_warning("Invalid ctx->logFile pointer!");
      return FALSE; //-- ERROR
    }
  if (NULL == ctx->keyFile)
    {
      g_warning("Invalid ctx->keyFile pointer!");
      return FALSE; //-- ERROR
    }

  g_info("cr_init_prg, ctx->logFile: %p, ctx->m: %d", (void *)(ctx->logFile), ctx->m);
  //dbg_hexdump("cr_Init_prg, ctx->sessionKey", (void *)(ctx->sessionKey), KEY_SIZE);

  size_t fileSize = (size_t) ctx->m * LOG_LEN; // m * LOG_LEN
  g_print("cr_Init_prg, fileSize: %ld, ctx->m: %d, LOG_LEN: %d\n", fileSize, ctx->m, LOG_LEN);

  // line 2
  if (cr_createNewLogFile(ctx->logFile, fileSize) == FALSE)
    {
      g_warning("Failed to create new log file!");
      return FALSE; //-- ERROR
    }

  // declare context outside of the file preperation, to reuse the given context if more then one file will be filled up. (not in this thesis)
  gboolean ret_randompad = FALSE;
  if (NULL == pp_prg)
    {
      //-- caller is not interested in cr_PRGContext instance
      g_info("cr_Init_prg: pp_prg == NULL, caller not interessted in cr_PRGContext");
      cr_PRGContext *prgContext = cr_CreatePRGContext(ctx->sessionKey);
      if (NULL == prgContext)
        {
          g_warning("Failed to create the PRGContext.");
          return FALSE; //-- ERROR
        }
      // line 4
      // write random pad.
      ret_randompad = cr_initializeLogFileWithPseudoRandomPad(prgContext, ctx->logFile, (size_t) ctx->m);
      g_free(prgContext);
    }
  else
    {
      if ( NULL == *pp_prg )
        {
          //-- first time called *pp_prg is expected to be NULL
          g_info("cr_Init_prg: *pp_prg== NULL, first call, caller is interessted in cr_PRGContext");
          *pp_prg =  cr_CreatePRGContext(ctx->sessionKey);
          if (NULL == *pp_prg)
            {
              g_warning("cr_Init_prg: Failed to create the initial PRGContext.");
              return FALSE; //-- ERROR
            }
          // line 4
          // write random pad.
          ret_randompad = cr_initializeLogFileWithPseudoRandomPad(*pp_prg, ctx->logFile, (size_t) ctx->m);
        }
      else
        {
          //-- called with an existing cr_PRGContext instance; do not create newly
          // line 4
          // write random pad.
          //-- Not the first time that cr_ini_prg is called
          g_info("cr_Init_prg: Re-use cr_PRGContext: *pp_prg: %p", (void *)(*pp_prg));
          ret_randompad = cr_initializeLogFileWithPseudoRandomPad(*pp_prg, ctx->logFile, (size_t) ctx->m);
        }
    }
  if (FALSE == ret_randompad)
    {
      g_warning("cr_initalizedLogFileWithPseudoRandomPad returns with error!");
      return FALSE; //-- ERROR
    }
  // First key evolution.
  // line 6
  cr_updateKey(ctx);
  return TRUE; //-- SUCCESS
}


/* ---------------------------------------------------------------------
 * cr_CreatePIContext
 *
 * in n (Max entries, count of plain log lines)
 * in is_backup_old_sessionkey
 * in szkeyFilePath (path to initial key to be used == base of session key)
 * in szOutputDirectoryPath (Note: expecting directory separator at the end!)
 * in szOutputEncLogPath (full file name (path) of encrypted output log file)
 *
 * returns cr_PIContext *  (Caller is owner, and must call free later)
 * NOTE: returns NULL in case of ERROR!
 */

cr_PIContext *cr_CreatePIContext(unsigned long n,
                                 gboolean is_backup_old_sessionkey,
                                 char *szKeyFilePath,
                                 char *szOutputDirectoryPath,
                                 char *szOutputEncLogPath)
{
  g_info("cr_CreatePIContext");
  if (n < THE_K)
    {
      g_warning("ERROR: cr_CreatePIContext: n (%ld) must be at least THE_K (%d)\n", n, THE_K);
      return NULL; //-- ERROR
    }

  if ( (NULL == szKeyFilePath) || (NULL == szOutputDirectoryPath) || (NULL == szOutputEncLogPath))
    {
      g_warning("ERROR: cr_CreatePIContext: invalid argument(s), NULL pointer not expected: %p, %p, %p\n",
                (void *)szKeyFilePath, (void *)szOutputDirectoryPath, (void *)szOutputEncLogPath);
      return NULL; //-- ERROR
    }

  cr_PIContext *ctx = g_try_new0(cr_PIContext, 1);
  if (NULL == ctx)
    {
      g_printerr("Critical: Failed to allocate memory for cr_PIContext.\n");
      return NULL; //-- ERROR
    }
  GString *gstr = g_string_new(NULL);
  unsigned char masterKey[KEY_SIZE];
  unsigned char sessionKey[KEY_SIZE];
  unsigned char tempKey[KEY_SIZE];
  memset(masterKey, 0, KEY_SIZE);
  memset(sessionKey, 0, KEY_SIZE);
  memset(tempKey, 0, KEY_SIZE);
  char szSessionKeyShortFileName[] = "currentSession.key";
  char *szSessionKeyPath = g_build_filename(szOutputDirectoryPath, szSessionKeyShortFileName, NULL);
  g_info("ctx: n: %ld", n);
  g_info("ctx: input key (master | host | initial): %s", szKeyFilePath);
  g_info("ctx: output directory: %s", szOutputDirectoryPath);
  g_info("ctx: session key path: %s", szSessionKeyPath);
  g_info("ctx: output file: %s", szOutputEncLogPath);

  if (access(szKeyFilePath, F_OK) == 0) //-- read master key
    {
      gboolean ret = cr_read_key(szKeyFilePath, masterKey); //-- read in the given input key and call it master key (in RAM)
      if (FALSE == ret)
        {
          g_warning("cr_CreatePIContext: Failed to read given key %s!\n", szKeyFilePath);
          return NULL; //-- ERROR
        }
    }
  else
    {
      g_warning("cr_CreatePIContext Key is not available: %s\n", szKeyFilePath);
      return NULL; //-- ERROR
    }

  //-- Check if the master key was named as the session key. If so, a backup
  //   must be done.
  gboolean is_input_named_as_session_key = FALSE;
  if (0 == strcmp(szKeyFilePath, szSessionKeyPath))
    {
      //-- Anyhow, keyFilePath is used as master / initial key
      g_warning("The inital / master key (%s) is named as a session key!", szKeyFilePath);
      is_input_named_as_session_key = TRUE;
    }

  //-- Check if there is already a key named as the session key available in file system
  if (access(szSessionKeyPath, F_OK) == 0)
    {
      char szTimestamp[256];
      memset(szTimestamp, 0, sizeof(szTimestamp));
      get_filename_timestamp(szTimestamp);
      char *sz_cp_path = NULL;
      GString *gstrName = NULL;
      gstrName = g_string_new("");
      //-- Check if the given input key is named like the session key
      if (TRUE == is_input_named_as_session_key)
        {
          g_string_printf(gstrName, "%s_%s%s", "master.key", szTimestamp, ".bak");
          g_warning("cr_CreatePIContext, Misleading Name, input Key is backuped into %s!", gstrName->str);
          sz_cp_path = g_build_filename(szOutputDirectoryPath, gstrName->str, NULL);
          if (NULL != sz_cp_path)
            {
              if (FALSE == cr_write_key(sz_cp_path, masterKey))
                {
                  g_warning("cr_CreatePIContext: Failed to write %s\n", sz_cp_path);
                }
              g_free(sz_cp_path);
              sz_cp_path = NULL;
            }
        }
      if (TRUE == is_backup_old_sessionkey)
        {
          g_string_printf(gstrName, "%s_%s%s", szSessionKeyShortFileName, szTimestamp, ".bak");
          g_info("cr_CreatePIContext: Session key already found and will be copied under a new name %s.", gstrName->str);
          sz_cp_path = g_build_filename(szOutputDirectoryPath, gstrName->str, NULL);
          g_string_printf(gstr, "Backup sessionkey to %s", sz_cp_path);
          if (NULL != sz_cp_path)
            {
              //-- copy file
              if (FALSE == cr_read_key(szSessionKeyPath, tempKey))
                {
                  g_warning("cr_CreatePIContext: Failed to read %s\n", szSessionKeyPath);
                }
              if (FALSE == cr_write_key(sz_cp_path, tempKey))
                {
                  g_warning("cr_CreatePIContext: Failed to write %s\n", sz_cp_path);
                }
              g_free(sz_cp_path);
              sz_cp_path = NULL;
            }
          //-- remove old session key
          g_info("unlink %s", szSessionKeyPath);
          unlink(szSessionKeyPath);
        }
      g_string_free(gstrName, TRUE);
      gstrName = NULL;
      g_free(sz_cp_path);
      sz_cp_path = NULL;
    }

  // Set key0
  memcpy(ctx->sessionKey, masterKey, KEY_SIZE);
  if (FALSE == cr_write_key(szSessionKeyPath, ctx->sessionKey) )
    {
      g_warning("cr_CreatePIContext: Failed to write %s\n", szSessionKeyPath);
    }
  if (access(szSessionKeyPath, F_OK) != 0)
    {
      g_warning("cr_CreatePIContext: Failed not available %s\n", szSessionKeyPath);
    }

  //-- Note: Just pointers are used sometimes in Logger context. No deep copy!
  ctx->keyPath = szSessionKeyPath;
  ctx->logFileDirectory = szOutputDirectoryPath;
  ctx->maxEntries = n;
  double temp_m = ceil(n * THE_C);
  ctx->m = (int) temp_m;
  ctx->logFileName = szOutputEncLogPath;

  //dbg_hexdump("ctx->sessionKey", (void *)(ctx->sessionKey), KEY_SIZE);
  //dbg_hexdump("masterKey", (void *)(masterKey), KEY_SIZE);

  //-- test read key and compare
  memset(tempKey, 5, KEY_SIZE);
  if (FALSE == cr_read_key(ctx->keyPath, tempKey))
    {
      g_warning("cr_CreatePIContext: Failed to read %s\n", ctx->keyPath);
    }
  if ( 0 != memcmp(masterKey, tempKey, KEY_SIZE))
    {
      g_warning("cr_CreatePIContext: Failed self test, tempKey (read back) must be the same as input key!");
    }
  else
    {
      g_info("cr_CreatePIContext: read session key successfully");
    }

  // open file handles are expected by caller
  ctx->keyFile = fopen(ctx->keyPath, "wb");
  if (NULL == ctx->keyFile)
    {
      g_string_printf(gstr, "Failed to open the key file. ctx->keyFile == NULL");
      g_free(szSessionKeyPath);
      g_free(ctx);
      ctx = NULL;
      g_warning("cr_CreatePIContext, %s\n", gstr->str);
      return NULL; //-- ERROR
    }

  //-- Precondition: An empty session key exits. It is written later in cr_Init function
  //   which is called directly after this function.
  if ((ctx->logFile = fopen(szOutputEncLogPath, "wb+")) == NULL)
    {
      int errnum = errno;
      g_string_printf(gstr, "opening file: %s, errnum: %s", szOutputEncLogPath, strerror(errnum));
      g_free(szSessionKeyPath);
      g_free(ctx);
      ctx = NULL;
      g_warning("cr_CreatePIContext %s\n", gstr->str);
      return NULL; //-- ERROR
    }

  g_string_printf(gstr, "n: %ld, ctx->logFile: %p, ctx->logFileName: %s", n, (void *) (ctx->logFile), ctx->logFileName);
  g_info("cr_CreatePIContext, %s", gstr->str);
  g_string_printf(gstr, "ctx->keyFile: %p, ctx->keyPath: %s", (void *)(ctx->keyFile), ctx->keyPath);
  g_info("cr_CreatePIContext, %s", gstr->str);
  g_string_printf(gstr, "ctx: %p", (void *)(ctx));
  g_info("cr_CreatePIContext, %s", gstr->str);
  g_string_free(gstr, TRUE);

  return ctx; //-- Note: Caller has to close later files and free ctx->keyPath
}


// helper method:
// create a new log file of the requested size.
// returns TRUE in case of SUCCESS else FALSE
//
gboolean cr_createNewLogFile(FILE *file, unsigned long fileSize)
{
  if (NULL == file)
    {
      g_warning("File is not opened.");
      return FALSE; //-- ERROR
    }

  // Seek to file size
  if (fseek(file, fileSize - 1, SEEK_SET) != 0)
    {
      g_warning("seeking to the end of the file");
      fclose(file);
      return FALSE; //-- ERROR
    }

  // Write a single byte to the end of the file to allocate space
  fputc('\0', file);

  return TRUE; //-- SUCCESS
}



// helper function:
// writes the random pad into the empty file.
// returns TRUE in case of SUCCESS else FALSE
//
gboolean cr_initializeLogFileWithPseudoRandomPad(cr_PRGContext *prg, FILE *file, size_t m)
{
  unsigned char buffer[LOG_LEN];
  if (NULL == prg)
    {
      g_warning("Invalid ctx pointer!");
      return FALSE; //-- ERROR
    }
  if (NULL == file)
    {
      g_warning("File is not opened.");
      return FALSE; //-- ERROR
    }
  fseek(file, 0, SEEK_SET);
  // write random pad "line" by "line".
  for (size_t i = 0; i < m; ++i)
    {
      if (cr_writePRGToFile(prg, file, buffer, LOG_LEN) != TRUE)
        {
          g_warning("Failed to write the random pad.");
          return FALSE; //-- ERROR
        }
    }
  return TRUE; //-- SUCCESS
}



// helper function
// writes a specific amount of pseudo random data into the provided file.
//
gboolean cr_writePRGToFile(cr_PRGContext *p_prg, FILE *file, unsigned char *buffer, size_t bufferSize)
{
  if (NULL == p_prg || NULL == file || NULL == buffer)
    {
      g_warning("cr_writePRGToFile: Invalid pointer!\n");
      return FALSE; //-- ERROR
    }
  // Create pseudo random data.
  if (cr_PRG(p_prg, buffer, bufferSize) == -1)
    {
      g_warning("Creating random PAD!\n");
      return FALSE; //-- ERROR
    }
  // write the random data into the file.
  size_t bytesWritten = fwrite(buffer, 1, bufferSize, file);
  if (bytesWritten != (size_t)bufferSize)
    {
      g_warning("While writing pseudo random PAD to the file!\n");
      fclose(file);
      return FALSE; //-- ERROR
    }
  return TRUE; //-- SUCCESS
}



// helper function:
// key evolution + updating the key file.
gboolean cr_updateKey(cr_PIContext *ctx)
{
  g_info("cr_updateKey");
  if (NULL == ctx)
    {
      g_warning("cr_updateKey: Invalid pointer ctx");
      return FALSE; //-- ERROR
    }
  if (NULL == ctx->keyFile)
    {
      g_warning("cr_updateKey: Invalid file pointer ctx->keyFile");
      return FALSE; //-- ERROR
    }
  unsigned char nextKey[KEY_SIZE];
  if ( 0 == cr_KeyEvolution(ctx->sessionKey, nextKey))
    {
      g_warning("cr_updateKey: Failed cr_KeyEvolution!");
      return FALSE; //-- ERROR
    }
  fseek(ctx->keyFile, 0, SEEK_SET);
  size_t writtenT = fwrite(nextKey, 1, KEY_SIZE, ctx->keyFile);
  if (writtenT != KEY_SIZE)
    {
      g_warning("Failed to write the key to the file.");
      return FALSE; //-- ERROR
    }
  memcpy(ctx->sessionKey, nextKey, KEY_SIZE);
  return TRUE; //-- SUCCESS
}



//----------------------------------------------------------------------
// cr_read_key
//
// retuns TRUE on SUCCESS and FALSE on FAILURE

gboolean cr_read_key(const char *path, unsigned char key[KEY_SIZE])
{
  FILE *file = fopen(path, "rb");
  if (!file)
    {
      g_warning("cr_read_key: Failed to open the key file.");
      return FALSE; //-- ERROR
    }
  if (fread(key, 1, KEY_SIZE, file) != KEY_SIZE)
    {
      g_warning("cr_read_key: fread can not read KEY_SIZE (%d) bytes!\n", KEY_SIZE);
      fclose(file);
      return FALSE; //-- ERROR
    }
  fclose(file);
  return TRUE; //-- SUCCESS
}



//----------------------------------------------------------------------
// cr_write_key
//
// in key
// in path
//
// retuns TRUE on SUCCESS and FALSE on FAILURE

gboolean cr_write_key(const char *path, unsigned char key[KEY_SIZE])
{
  FILE *file = fopen(path, "wb");
  if (file == NULL)
    {
      g_warning("cr_write_key, Failed to open the key file.");
      return FALSE; //-- ERROR
    }
  size_t writtenT = fwrite(key, 1, KEY_SIZE, file);
  fflush(file);
  int fd = fileno(file); //-- Get the underlying file descriptor
  fsync(fd); //-- Write it now
  if (writtenT != KEY_SIZE)
    {
      g_warning("cr_write_key, Failed to write the key to the file");
      fclose(file);
      return FALSE; //-- ERROR
    }
  fclose(file);
  return TRUE; //-- SUCCESS
}



//----------------------------------------------------------------------
// cr_encryptLog
//
// Will AE$ encrypt the provided log message, by uisng AES_256_CTR + CMAC.
//
// in key: the current session key for encryption
// in logMessage: the UTF-8 log message to encrypt as byte counter
// in logMessageSize: the size in octets of the log message.
// cipherLogMessage: (ci) the ciphertext, always of the size IV + MESSAGE_LEN_SLOGCR + MAC_LEN.
//
// NOTE: logMessageSize is count in octets which is NOT count of symbols / characters
// due to UTF-8 support.
// The caller is expected having ensured that only valid characters are
// provided after trunctaion to allowed octet length.
//
// return: 0 on failure else IV_SIZE + MESSAGE_LEN_SLOGCR + MAC_LEN on success.

int cr_encryptLog(const unsigned char *key, const unsigned char *logMessage, size_t logMessageSize,
                  unsigned char *cipherLogMessage)
{
  unsigned char iv[IV_SIZE];
  unsigned char paddedMessage[MESSAGE_LEN_SLOGCR];
  unsigned char ciphertext[MESSAGE_LEN_SLOGCR];
  unsigned char mac[MAC_LEN];
  int len;
  size_t macLen;

  cr_GenerateIV(iv);

  //-- padding with 0 bytes and limitation to MESSAGE_LEN_SLOGCR
  //   Caller is responsible that limitation ensures valid UTF-8 character!
  size_t possibly_shortened_size = (size_t)MIN((size_t)logMessageSize, (size_t)MESSAGE_LEN_SLOGCR); //-- safe
  memset(paddedMessage, 0, sizeof(paddedMessage));
  memcpy(paddedMessage, logMessage, possibly_shortened_size);

  // encrypt the log message.
  if (MESSAGE_LEN_SLOGCR != (len = cr_AES_256_CTR_encrypt(paddedMessage, MESSAGE_LEN_SLOGCR, (unsigned char *)key, iv, ciphertext)))
    {
      g_warning("Log encyption failed.\n");
      return 0;
    }

  memcpy(cipherLogMessage, iv, IV_SIZE);
  memcpy(cipherLogMessage + IV_SIZE, ciphertext, MESSAGE_LEN_SLOGCR);

  // Create the MAC.
  cr_CMAC((guchar *)key, ciphertext, (gsize)MESSAGE_LEN_SLOGCR, mac, &macLen, (gsize)MAC_LEN);

  memcpy(cipherLogMessage + IV_SIZE + MESSAGE_LEN_SLOGCR, mac, MAC_LEN);

  // Return cipherLogMessage len.
  return IV_SIZE + MESSAGE_LEN_SLOGCR + MAC_LEN;
}



/* ---------------------------------------------------------------------
 * init_cr_logger_functionality
 *
 * in  p_loggerctx: struct with file and directory information setup
 * out pp_pictx: Initalized cr_PIContext
 *
 * returns TRUE in case SUCCESS, else FALSE
 */

gboolean init_cr_logger_functionality(cr_pi_logger_context *p_loggerctx, cr_PIContext **pp_pictx,
                                      cr_PRGContext **pp_prg)
{
  if (NULL == p_loggerctx)
    {
      g_printerr("ERROR: NULL pointer: p_loggerctx!\n");
      return FALSE; //-- ERROR
    }
  if (NULL == pp_pictx)
    {
      g_printerr("ERROR: NULL pointer: pp_pictx!\n");
      return FALSE; //-- ERROR
    }
  if (p_loggerctx->maxLogs <= THE_K)
    {
      g_printerr("ERROR: p_loggerctx->maxLogs must be greater then THE_K (%d)!\n", THE_K);
      return FALSE; //-- ERROR
    }
  //const gsize BLOCKSIZE = MESSAGE_LEN_SLOGCR + IV_SIZE + MAC_LEN + INTEGRITY_TAG_LEN + ID_LEN;
  //-- e.g. MESSAGE_LEN_SLOGCR + 64 = 2048 + 64 = 2112
  //gdouble result = p_loggerctx->maxLogs * THE_C; // maxLogs == the_n, here Count of log lines in log file until log rot
  //gdouble temp = ceil(result);
  //gsize the_m = (gsize)temp;
  *pp_pictx = cr_CreatePIContext(p_loggerctx->maxLogs,
                                 TRUE,
                                 p_loggerctx->p_MasterKeyPath,
                                 p_loggerctx->p_OutputDirectoryPath,
                                 p_loggerctx->p_OutputEncLogPath);
  if (NULL == *pp_pictx)
    {
      g_printerr("ERROR: cr_pi_logger: cr_CreatePIContexted failed!\n");
      return  FALSE; //-- ERROR
    }

  if (NULL == pp_prg)
    {
      g_info("pp_prg is NULL, caller is not interessed in prg");
      //-- caller is not interessted in prg
      if (FALSE == cr_Init_prg(*pp_pictx, NULL))
        {
          g_printerr("ERROR: Initialization failed!\n");
          g_free((*pp_pictx)->keyPath);
          g_free(*pp_pictx);
          *pp_pictx = NULL;
          return FALSE; //-- ERROR
        }
    }
  else
    {
      g_info("pp_prg is not NULL and caller is interessted in prg");
      if (FALSE == cr_Init_prg(*pp_pictx, pp_prg))
        {
          g_printerr("ERROR: cr_Init_prg fails!");
          g_free((*pp_pictx)->keyPath);
          g_free(*pp_pictx);
          *pp_pictx = NULL;
          g_free(*pp_prg);
          *pp_prg = NULL;
          return FALSE; //-- ERROR
        }
    }
  g_info("Done successfully: cr_Init_prg");
  return TRUE; //-- SUCCESS
}

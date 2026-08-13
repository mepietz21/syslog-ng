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


#ifndef cr_pi_logger_h
#define cr_pi_logger_h

#include <stdio.h>
#include <glib.h>

#include "cr_pi_logger_context.h"
#include "cr_pi_shared.h" //-- macros, #define
#include "cr_crypto.h"



typedef struct
{
  unsigned char sessionKey[KEY_SIZE]; // Contains the current session key.

  // old school:
  char *keyPath; // Path to the current seassion key.
  char *logFileDirectory; // Directory holding the log files.
  char *logFileName; // New: Full filename of encrypted output file. Old: Short file name prefix without extension.
  // TODO use glib style and GString*
  // GString* gstr_session_key_path;  ..
  unsigned long maxEntries; // (n) Maximum number of logs the file could hold.
  int m; // m = n * c. (c -> THE_C, K -> THE_K)
  FILE *logFile; // File pointer of the log file.
  FILE *keyFile; // File pointer of the session key file.
} cr_PIContext;



/*
 * Function: CreatePIContext
 * -------------------------
 * Will create a struct of type PIContext, based on the provided informations.
 *
 * in n : The maximum number of log entries, that can be stored in a single file.
 * in is_backup_old_sessionke Whether exisiting session key shall be backuped or just overwritten
 * in szkeyFilePath : Full file name (path) of initial key file.
 * in szOutputDirectoryPath: Output directoy path
 * in szOutputEncLogPath: Full file name (path) of encrypted to be created output log file
 *
 * returns: a new struct of type PIContext, initialized with the requested parameters.
 */
cr_PIContext *cr_CreatePIContext(unsigned long n,
                                 gboolean is_backup_old_sessionkey,
                                 char *szKeyFilePath,
                                 char *szOutputDirectoryPath,
                                 char *szOutputEncLogPath);

/*
 * Function: Init
 * --------------
 * Based on the provided context a new log file will be created.
 *
 * ctx: Logger Context.
 */
gboolean cr_Init(cr_PIContext *ctx);

//-- same as cr_Init but prg can be created or just used
// This function behaves like cr_Init when prg is NULL
// If a valid pointer is provided, caller is responsible of
// the allocated memory.
gboolean cr_Init_prg(cr_PIContext *ctx, cr_PRGContext **prg);


/*
 * Function: AddLogEntry
 * ---------------------
 * Will add a new log entry to the log file.
 *
 * ctx:  Logger Context.
 * logMessage: Log message that will be logged.
 * logMessageSize: size of the message, messages longer than the max log legth, will be truncated.
 *
 * returns: FALSE on failure and TRUE on sucess.
 */
gboolean cr_AddLogEntry(cr_PIContext *ctx, const unsigned char *logMessage, size_t logMessageSize);

// TODO prosa

gboolean cr_read_key(const char *path, unsigned char key[KEY_SIZE]);
gboolean cr_write_key(const char *path, unsigned char key[KEY_SIZE]);
gboolean cr_createNewLogFile(FILE *file, size_t fileSize);
gboolean cr_initializeLogFileWithPseudoRandomPad(cr_PRGContext *p_prg, FILE *file, size_t m);
gboolean cr_writePRGToFile(cr_PRGContext *p_prg, FILE *file, unsigned char *buffer, size_t bufferSize);
gboolean cr_updateKey(cr_PIContext *ctx);
int cr_encryptLog(const unsigned char *key, const unsigned char *logMessage, size_t logMessageSize, unsigned char *cipherLogMessage);
gboolean init_cr_logger_functionality(cr_pi_logger_context *p_loggerctx, cr_PIContext **pp_pictx,
                                      cr_PRGContext **pp_prg);


#endif /* cr_pi_logger_h */


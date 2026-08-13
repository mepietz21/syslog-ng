/*
 * Copyright (c) 2015-2026 Airbus Commercial Aircraft
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

/* File: cr_destination.h */


#ifndef CR_DESTINATION_H
#define CR_DESTINATION_H

#include <glib.h> // For GMutex
#include <math.h>

#include "driver.h"
#include "logthrdest/logthrdestdrv.h"

#include "cr_pi_shared.h"
#include "cr_pi_logger.h"
#include "cr_pi_logger_context.h"

typedef enum
{
  CR_LOGMODE_DIRECT = 0,
  CR_LOGMODE_BASE64,
  CR_LOGMODE_ENC
} CrLogMode;

typedef struct
{
  LogThreadedDestDriver super; /* The parent "class" */
  GString *gstr_filenametoken;
  GString *gstr_keypath;
  GString *gstr_dir;
  GString *gstr_mode;
  GMutex lock;                 /* Mutex for synchronization */
  FILE *fp_current_file_plain; /* The currently open file pointer */
  gsize n_message_count;        /* The shared, global counter */
  gsize n_logrotcnt;  /* log rotation by number of entries */
  gsize n_current_part;         /* The current file sequence number (part1, part2, etc.) */

  CrLogMode log_mode;

  /* Crash Recovery */
  cr_pi_logger_context loggerctx;
  cr_PIContext *p_pictx;
  cr_PRGContext *p_prg;
  gboolean is_reuse_prg;
} CrDestinationDriver;

LogDriver *cr_destination_dd_new(GlobalConfig *cfg);

void cr_destination_dd_set_filenametoken(LogDriver *d, const gchar *value);
void cr_destination_dd_set_keypath(LogDriver *d, const gchar *value);
void cr_destination_dd_set_dir(LogDriver *d, const gchar *value);
void cr_destination_dd_set_mode(LogDriver *d, const gchar *value);
void cr_destination_dd_set_logrotcnt(LogDriver *d, const gsize value);

/* Crash Recovery */

/* helper to clenup when log rotation takes place and on
 * syslog-ng shutdown */
void cr_destination_dd_free_logger_pi_contexts(CrDestinationDriver *self);

/* helper to printf debug */
void cr_destination_dd_debug_log(CrDestinationDriver *self);

#endif /* CR_DESTINATION_H */


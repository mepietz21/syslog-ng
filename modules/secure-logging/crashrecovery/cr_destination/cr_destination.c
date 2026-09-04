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

/* File: cr_destination.c */

#include "cr_destination.h"
#include "cr_destination_worker.h"
#include "cr_destination-parser.h"
#include "limits.h"
#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats/stats-registry.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"
#include "logthrdest/logthrdestdrv.h"
#include "cfg.h"          // Configuration context (LogDriver->configuration)
#include "template/templates.h"

#include "cr_pi_logger_context.h"
#include "cr_pi_logger.h"


// new
static CrLogMode
_cr_destination_parse_logmode(const gchar *mode)
{
  if (mode == NULL)
    return CR_LOGMODE_ENC;

  if (g_ascii_strcasecmp(mode, "direct") == 0)
    return CR_LOGMODE_DIRECT;

  if (g_ascii_strcasecmp(mode, "base64") == 0)
    return CR_LOGMODE_BASE64;

  if (g_ascii_strcasecmp(mode, "enc") == 0)
    return CR_LOGMODE_ENC;

  return CR_LOGMODE_ENC;
}

//----------------------------------------------------------------------
// cr_destination_dd_set_filenametoken
// Take over configuration of file name token (first part of full file name)
// This path will be later expanded with log rotataion information.

void cr_destination_dd_set_filenametoken(LogDriver *d, const gchar *value)
{
  CrDestinationDriver *self = (CrDestinationDriver *)d;
  g_string_assign(self->gstr_filenametoken, value);
  msg_info(CR_INFO_PREFIX, evt_tag_str("cr_destination filenametoken", self->gstr_filenametoken->str));
}


//----------------------------------------------------------------------
// cr_destination_dd_set_keypath
// Take over configuration of initial key path
// The key behind this path will later be read and stored as session key.

void cr_destination_dd_set_keypath(LogDriver *d, const gchar *value)
{
  CrDestinationDriver *self = (CrDestinationDriver *)d;
  g_string_assign(self->gstr_keypath, value);
  msg_info(CR_INFO_PREFIX, evt_tag_str("cr_destination keypath", self->gstr_keypath->str));
}


//----------------------------------------------------------------------
// cr_destination_dd_set_dir
// Take over configuration of directory path where to store files

void cr_destination_dd_set_dir(LogDriver *d, const gchar *value)
{
  CrDestinationDriver *self = (CrDestinationDriver *)d;
  g_string_assign(self->gstr_dir, value);
  msg_info(CR_INFO_PREFIX, evt_tag_str("cr_destination dir", self->gstr_keypath->str));
}


//----------------------------------------------------------------------
// cr_destination_dd_set_logrotcnt
// Take over the count of log lines allowed for log file (log rotation size)

void
cr_destination_dd_set_logrotcnt(LogDriver *d, const gsize value)
{
  CrDestinationDriver *self = (CrDestinationDriver *) d;

  self->n_logrotcnt = value;
  self->loggerctx.maxLogs = value;

  msg_info(CR_INFO_PREFIX,
           evt_tag_long("cr_destination logrotcnt", (glong) value));
}

//----------------------------------------------------------------------
// cr_destination_dd_set_mode
// Take over configuration of log mode.
// Note: Here on contrary to secure-logging, log mode plain does NOT
// provide a checksum. Only log rotation is offered.
// Log mode 'enc' does provide Crash Recovery logging without providing
// plain logs. Log mode 'plain_enc' provides plain and Crash Recovery
// logging.

// new
void
cr_destination_dd_set_mode(LogDriver *d, const gchar *value)
{
  CrDestinationDriver *self = (CrDestinationDriver *) d;

  self->log_mode = _cr_destination_parse_logmode(value);
  g_string_assign(self->gstr_mode, value ? value : "enc");

  msg_info(CR_INFO_PREFIX,
           evt_tag_str("cr_destination mode", self->gstr_mode->str),
           evt_tag_long("log_mode", self->log_mode));
}


/*
 * Utilities
 */

static const gchar *_format_stats_key(LogThreadedDestDriver *d, StatsClusterKeyBuilder *kb)
{
  CrDestinationDriver *self = (CrDestinationDriver *)d;
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "cr_destination"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("filename", self->gstr_filenametoken->str));
  return NULL;
}

static const gchar *_format_persist_name(const LogPipe *d)
{
  CrDestinationDriver *self = (CrDestinationDriver *)d;
  static gchar persist_name[PATH_MAX];
  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name) - 1, "cr_destination.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name) - 1, "cr_destination.%s", self->gstr_filenametoken->str);
  return persist_name;
}

//----------------------------------------------------------------------
// _dd_init
// Take over configuration data and create filenames and contexts for cr
// logging

static gboolean _dd_init(LogPipe *d)
{
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_init"));
  CrDestinationDriver *self = (CrDestinationDriver *)d;
  if (!log_threaded_dest_driver_init_method(d))
    {
      return FALSE;
    }
  if (!self->gstr_filenametoken->len)
    {
      msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_init, invalid gstr_filenametoken"));
      g_string_assign(self->gstr_filenametoken, "/tmp/WTF_FINDME_cr_destination-output.enc");
    }
  g_mutex_init(&self->lock);
  self->is_reuse_prg = FALSE; //-- always, never use old prg for other log files. Refresh context always.

  //-- TODO DRY same functionality is needed in worker

  self->n_message_count = 0;
  self->n_current_part = 1; // Start with part 1

  //-- TODO check mode

  if (self->log_mode == CR_LOGMODE_DIRECT ||
      self->log_mode == CR_LOGMODE_BASE64)
    {
      const gchar *extension =
          self->log_mode == CR_LOGMODE_BASE64 ? "b64" : "log";

      gchar *filename =
          g_strdup_printf("%s_part%ld.%s",
                          self->gstr_filenametoken->str,
                          self->n_current_part,
                          extension);

      self->fp_current_file_plain = fopen(filename, "a");

      msg_info(CR_INFO_PREFIX,
                evt_tag_str("filename", filename),
                evt_tag_str("mode", self->gstr_mode->str));

      g_free(filename);

      if (!self->fp_current_file_plain)
        {
          msg_error(CR_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "cr_destination, Could not open new output file"));
          return FALSE;
        }
    }
  else if (self->log_mode == CR_LOGMODE_ENC)
    {
      self->loggerctx.maxLogs = self->n_logrotcnt;

      self->loggerctx.p_OutputEncLogPath =
          g_strdup_printf("%s_part%ld.enc",
                          self->gstr_filenametoken->str,
                          self->n_current_part);

      self->loggerctx.p_MasterKeyPath =
          self->gstr_keypath->str;

      self->loggerctx.p_OutputDirectoryPath =
          self->gstr_dir->str;

      self->p_pictx = NULL;

      gboolean cr_retval;

      if (self->is_reuse_prg)
        {
          cr_retval =
              init_cr_logger_functionality(&(self->loggerctx),
                                            &(self->p_pictx),
                                            &(self->p_prg));
        }
      else
        {
          cr_retval =
              init_cr_logger_functionality(&(self->loggerctx),
                                            &(self->p_pictx),
                                            NULL);
        }

      if (FALSE == cr_retval)
        {
          msg_error(CR_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "cr_destination, init_cr_logger_functionality"));
          return FALSE;
        }
    }
  else
    {
      msg_error(CR_ERROR_PREFIX,
                evt_tag_str("Reason",
                            "cr_destination, invalid log mode"),
                evt_tag_str("mode", self->gstr_mode->str));
      return FALSE;
    }

  cr_destination_dd_debug_log(self);

  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_init, DONE"));
  return TRUE;
}


//----------------------------------------------------------------------
// _dd_deinit

gboolean _dd_deinit(LogPipe *s)
{
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_deinit"));
  /* If you created resources during init,  you need to destroy them here. */
  gboolean ret =  log_threaded_dest_driver_deinit_method(s);
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_deinit, DONE"), evt_tag_long("ret", (gint) ret));
  return ret;
}


//----------------------------------------------------------------------
// _dd_free
// Free dyn filenames and cr logging contexts

static void _dd_free(LogPipe *d)
{
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_free"));
  CrDestinationDriver *self = (CrDestinationDriver *)d;

  //-- plain
  if (self->fp_current_file_plain)
    {
      fflush(self->fp_current_file_plain);
      fclose(self->fp_current_file_plain);
      self->fp_current_file_plain = NULL;
    }

  //-- enc
  cr_destination_dd_free_logger_pi_contexts(self);

  //-- free values from bison parser / conf file
  g_string_free(self->gstr_filenametoken, TRUE);
  g_string_free(self->gstr_dir, TRUE);
  g_string_free(self->gstr_keypath, TRUE);
  g_string_free(self->gstr_mode, TRUE);

  g_mutex_clear(&self->lock);
  log_threaded_dest_driver_free(d);

  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination, _dd_free, DONE"));
}


//----------------------------------------------------------------------
// cr_destination_dd_new
// Create CrDestinationDriver
// Link functions
// Initialize members with NULL

LogDriver *cr_destination_dd_new(GlobalConfig *cfg)
{
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination_dd_new"));
  CrDestinationDriver *self = g_new0(CrDestinationDriver, 1);

  //-- plain + cr
  self->gstr_filenametoken = g_string_new("");
  self->gstr_keypath = g_string_new("");
  self->gstr_dir = g_string_new("");
  // new
  self->gstr_mode = g_string_new("");
  self->log_mode = CR_LOGMODE_ENC;

  //-- cr
  self->p_pictx = NULL;
  self->loggerctx.p_OutputEncLogPath = NULL;
  self->loggerctx.p_MasterKeyPath = NULL;
  self->loggerctx.p_OutputDirectoryPath = NULL;
  self->loggerctx.p_InputPlainLogPath = NULL; //-- used only in standalone cr_logger app
  self->loggerctx.maxLogs = 7; //-- min value must be greate than THE_K

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = _dd_init;
  self->super.super.super.super.deinit = _dd_deinit;
  self->super.super.super.super.free_fn = _dd_free;

  self->super.format_stats_key = _format_stats_key;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.stats_source = stats_register_type("cr_destination");
  self->super.worker.construct = cr_destination_dw_new;

  return (LogDriver *)self;
}


//----------------------------------------------------------------------
// cr_destination_dd_free_logger_pi_contexts
// - close files (key, log)
// - free dyn created objects
// - set members to NULL

void cr_destination_dd_free_logger_pi_contexts(CrDestinationDriver *self)
{
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination_dd_free_logger_pi_contexts"));
  if (NULL != self)
    {
      msg_info(CR_INFO_PREFIX, evt_tag_str("self", "cr_destination_dd_free_logger_pi_contexts"));
      self->loggerctx.p_OutputDirectoryPath = NULL; //self->gstr_dir->str;
      self->loggerctx.p_MasterKeyPath = NULL; // self->gstr_keypath->str;
      //-- OutputEncLogPath is a constructed value, not just a pointer copy
      g_free(self->loggerctx.p_OutputEncLogPath);
      self->loggerctx.p_OutputEncLogPath = NULL;
      if (NULL != self->p_pictx)
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("p_pictx", "cr_destination_dd_free_logger_pi_contexts"));
          if (NULL != self->p_pictx->logFile)
            {
              msg_info(CR_INFO_PREFIX, evt_tag_str("logFile", "cr_destination_dd_free_logger_pi_contexts"));
              fflush(self->p_pictx->logFile);
              fclose(self->p_pictx->logFile);
              self->p_pictx->logFile = NULL;
            }
          if (NULL != self->p_pictx->keyFile)
            {
              msg_info(CR_INFO_PREFIX, evt_tag_str("keyFile", "cr_destination_dd_free_logger_pi_contexts"));
              fflush(self->p_pictx->keyFile);
              fclose(self->p_pictx->keyFile);
              self->p_pictx->keyFile = NULL;
            }
          g_free(self->p_pictx->keyPath); //-- sessionkey, contstructed value
          g_free(self->p_pictx);
          self->p_pictx = NULL;
        }
      if (FALSE == self->is_reuse_prg)
        {
          if (NULL != self->p_prg)
            {
              g_free(self->p_prg);
              self->p_prg = NULL;
            }
        }
    }
  msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", "cr_destination_dd_free_logger_pi_contexts DONE"));
}

//----------------------------------------------------------------------
// cr_destination_dd_debug_log
// Helper for printf debugging

void cr_destination_dd_debug_log(CrDestinationDriver *self)
{
  if (NULL != self)
    {
      GString *gstr = g_string_new(NULL);
      if (NULL != self->gstr_filenametoken)
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->gstr_filenametoken->str",  self->gstr_filenametoken->str));
        }
      else
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->gstr_filenametoken",  "NULL"));
        }
      msg_info(CR_INFO_PREFIX, evt_tag_long("self->n_current_part", self->n_current_part));
      g_string_printf(gstr, "%p", (void *)(self->fp_current_file_plain));
      msg_info(CR_INFO_PREFIX, evt_tag_str("self->fp_current_file_plain", gstr->str));
      g_string_truncate(gstr, 0);
      msg_info(CR_INFO_PREFIX, evt_tag_long("self->loggerctx.maxLogs",  self->loggerctx.maxLogs));
      if (NULL != self->loggerctx.p_OutputEncLogPath)
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->loggerctx.p_OutputEncLogPath", self->loggerctx.p_OutputEncLogPath));
        }
      else
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->loggerctx.p_OutputEncLogPath", NULL));
        }
      if (NULL != self->loggerctx.p_MasterKeyPath)
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->loggerctx.p_MasterKeyPath", self->loggerctx.p_MasterKeyPath));
        }
      else
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->loggerctx.p_MasterKeyPath", NULL));
        }
      if (NULL != self->loggerctx.p_OutputDirectoryPath)
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->loggerctx.p_OutputDirectoryPath", self->loggerctx.p_OutputDirectoryPath));
        }
      else
        {
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->loggerctx.p_OutputDirectoryPath", NULL));
        }
      g_string_printf(gstr, "%p", (void *) (self->p_pictx));
      msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx", gstr->str));
      if (NULL != self->p_pictx)
        {
          g_string_printf(gstr, "%p", (void *) (self->p_pictx->keyPath));
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->keyPath", gstr->str));
          g_string_truncate(gstr, 0);
          if (NULL != self->p_pictx->keyPath)
            {
              msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->keyPath", self->p_pictx->keyPath));
            }
          g_string_printf(gstr, "%p", (void *) (self->p_pictx->logFileDirectory));
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->logFileDirectory", gstr->str));
          g_string_truncate(gstr, 0);
          if (NULL != self->p_pictx->logFileDirectory)
            {
              msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->logFileDirectory", self->p_pictx->logFileDirectory));
            }
          g_string_printf(gstr, "%p", (void *) (self->p_pictx->logFileName));
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->logFileName", gstr->str));
          g_string_truncate(gstr, 0);
          if (NULL != self->p_pictx->logFileName)
            {
              msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->logFileName", self->p_pictx->logFileName));
            }
          msg_info(CR_INFO_PREFIX, evt_tag_long("self->p_pictx->maxEntries",  self->p_pictx->maxEntries));
          msg_info(CR_INFO_PREFIX, evt_tag_long("self->p_pictx->m",  self->p_pictx->m));
          g_string_printf(gstr, "%p", (void *) (self->p_pictx->logFile));
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->logFile", gstr->str));
          g_string_truncate(gstr, 0);
          g_string_printf(gstr, "%p", (void *) (self->p_pictx->keyFile));
          msg_info(CR_INFO_PREFIX, evt_tag_str("self->p_pictx->keyFile", gstr->str));
          g_string_truncate(gstr, 0);
        }
      g_string_printf(gstr, "%p", (void *) (self->p_prg));
      msg_info(CR_INFO_PREFIX, evt_tag_str("(always NULL when no reuse of prg, self->p_prg", gstr->str));
      g_string_free(gstr, TRUE);
    }
}
/*
 * Copyright (c) 2025-2026 Airbus Commercial Aircraft
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

#include "cr_destination_worker.h"
#include "cr_destination.h"
#include "thread-utils.h"
#include "template/eval.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include <stdio.h>
#include <locale.h>
#include <string.h>

// In cr_destination_worker.c

#define CRMDW "cr_destination_worker"

static LogThreadedResult
_dw_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  // CrDestinationWorker *self = (CrDestinationWorker *)s;
  CrDestinationDriver *owner = (CrDestinationDriver *)s->owner;
  LogThreadedResult result = LTR_SUCCESS;
  GString *gstr_log = g_string_new("");

  msg_info(CR_INFO_PREFIX,
           evt_tag_str("Reason", CRMDW ", _dw_insert"),
           evt_tag_long("count", owner->n_message_count),
           evt_tag_long("logrotcnt", owner->n_logrotcnt),
           evt_tag_long("part", owner->n_current_part));

  g_mutex_lock(&owner->lock);

  if (THE_K >= owner->n_logrotcnt)
    {
      msg_error(CR_ERROR_PREFIX,
                evt_tag_str("Reason", CRMDW ", Invalid value of owner->n_logrotcnt!"));

      result = LTR_NOT_CONNECTED;
      goto CLEANUP_DW_INSERT;
    }

  g_info("owner->n_message_count: %ld, owner->n_logrotcnt: %ld", owner->n_message_count, owner->n_logrotcnt);

  if (owner->n_message_count >= owner->n_logrotcnt)
    {
      g_info("Log rotation, new file initialization");

      //-- Handle log rotation (initialization of new log files)

      //-- TODO handle mode currently ignored. Both plain and enc.

      //-- Close the old plain file ---
      if (owner->fp_current_file_plain)
        {
          fflush(owner->fp_current_file_plain);
          fclose(owner->fp_current_file_plain);
          owner->fp_current_file_plain = NULL;
        }

      //-- Update state (Increment part, reset count)
      owner->n_current_part++;
      owner->n_message_count = 0; //-- count per current file
  if (owner->log_mode == CR_LOGMODE_DIRECT ||
          owner->log_mode == CR_LOGMODE_BASE64)
        {
          //-- Prepare the new plain file ---
          const gchar *extension =
              owner->log_mode == CR_LOGMODE_BASE64 ? "b64" : "log";

          gchar *sz_new_filename_plain =
              g_strdup_printf("%s_part%ld.%s",
                              owner->gstr_filenametoken->str,
                              owner->n_current_part,
                              extension);

          owner->fp_current_file_plain = fopen(sz_new_filename_plain, "a");
          msg_info(CR_INFO_PREFIX,
                   evt_tag_str("sz_new_filename_plain", sz_new_filename_plain));
          g_free(sz_new_filename_plain);

          if (!owner->fp_current_file_plain)
            {
              msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW", Could not open new file after rotation!"));
              result = LTR_NOT_CONNECTED;
              goto CLEANUP_DW_INSERT;
            }
        }
      else if (owner->log_mode == CR_LOGMODE_ENC)
        {
          //-- Prepare the new enc file ---
          //-- The old contexts are deleted. Each log file starts with the same key
          cr_destination_dd_free_logger_pi_contexts(owner);
          owner->loggerctx.maxLogs = owner->n_logrotcnt;
          owner->loggerctx.p_OutputEncLogPath = g_strdup_printf("%s_part%ld.enc", owner->gstr_filenametoken->str,
                                                                owner->n_current_part);
          msg_info(CR_INFO_PREFIX, evt_tag_str("owner->gstr_keypath->str",
                                               owner->gstr_keypath->str)); //-- same key for each log file
          owner->loggerctx.p_MasterKeyPath = owner->gstr_keypath->str;
          owner->loggerctx.p_OutputDirectoryPath = owner->gstr_dir->str;
          owner->p_pictx = NULL;

          gboolean cr_retval = FALSE;
          if (owner->is_reuse_prg)
            {
              if (NULL == owner->p_prg)
                {
                  msg_warning(CR_WARNING_PREFIX, evt_tag_str("Reason", CRMDW", p_prg is NULL which is not expected at this stage"));
                }
              //-- Create new pi context, init
              cr_retval = init_cr_logger_functionality(&(owner->loggerctx), &(owner->p_pictx), &(owner->p_prg));
            }
          else
            {
              cr_retval = init_cr_logger_functionality(&(owner->loggerctx), &(owner->p_pictx), NULL);
            }

          cr_destination_dd_debug_log(owner);

          if (FALSE == cr_retval)
            {
              msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW", init_cr_logger_functionality"));
              result = LTR_NOT_CONNECTED;
              goto CLEANUP_DW_INSERT;
            }
        }
    } //-- File handling for log rotation

  //-- get message ---
  g_string_printf(gstr_log, "%s\n", log_msg_get_value(msg, LM_V_RAWMSG, NULL));
  // msg_info(CR_INFO_PREFIX, evt_tag_str("mmmmm gstr_log->str", gstr_log->str));

    if (owner->log_mode == CR_LOGMODE_ENC)
    {
      //-- write enc ---
      if ((NULL != owner->p_pictx) && (NULL != owner->p_pictx->logFile))
        {
          gboolean is_add = cr_AddLogEntry(owner->p_pictx, (unsigned char *)(gstr_log->str), gstr_log->len);
          if (FALSE == is_add)
            {
              msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW", _dw_insert, cr_AddLogEntry failed!"));
              result = LTR_NOT_CONNECTED;
              goto CLEANUP_DW_INSERT;
            }
        }
      else
        {
          msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW",  _dw_insert, invalid pi context (owner->p_pictx)!"));
          result = LTR_NOT_CONNECTED;
          goto CLEANUP_DW_INSERT;
        }
    }
  else if (owner->log_mode == CR_LOGMODE_DIRECT)
    {
      //-- write plain ---
      if (fwrite(gstr_log->str, 1, gstr_log->len, owner->fp_current_file_plain) != gstr_log->len ||
          fflush(owner->fp_current_file_plain) != 0)
        {
          msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW", _dw_insert: fwrite | fflush failed!"));
          result = LTR_NOT_CONNECTED;
          goto CLEANUP_DW_INSERT;
        }
    }
  else if (owner->log_mode == CR_LOGMODE_BASE64)
    {
      //-- write base64 ---
      gchar *base64_log = g_base64_encode((const guchar *)gstr_log->str, gstr_log->len);

      if (NULL == base64_log)
        {
          msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW", _dw_insert: g_base64_encode failed!"));
          result = LTR_NOT_CONNECTED;
          goto CLEANUP_DW_INSERT;
        }

      gsize base64_len = strlen(base64_log);

      if (fwrite(base64_log, 1, base64_len, owner->fp_current_file_plain) != base64_len ||
          fflush(owner->fp_current_file_plain) != 0)
        {
          msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", CRMDW", _dw_insert: fwrite | fflush failed!"));
          g_free(base64_log);
          result = LTR_NOT_CONNECTED;
          goto CLEANUP_DW_INSERT;
        }

      g_free(base64_log);
    }

  owner->n_message_count++; // Success: Increment the counter

  msg_info(CR_INFO_PREFIX,
           evt_tag_str("Success", CRMDW ", _dw_insert"),
           evt_tag_long("count after log written", owner->n_message_count),
           evt_tag_long("part", owner->n_current_part));

CLEANUP_DW_INSERT:
  if (gstr_log)
    {
      g_string_free(gstr_log, TRUE);
    }

  g_mutex_unlock(&owner->lock);
  return result;
}

  static gboolean _connect(LogThreadedDestWorker *s)
  {
    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", "cr_destination, _connect"));
    return TRUE;
  }

  static void _disconnect(LogThreadedDestWorker *s)
  {
    //CrDestinationWorker *self = (CrDestinationWorker *)s;
    CrDestinationDriver *owner = (CrDestinationDriver *)s->owner;
    msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", CRMDW", _disconnect"));
    if (owner->fp_current_file_plain)
    {
      fflush(owner->fp_current_file_plain);
    }

    if ((NULL != owner->p_pictx) && (NULL != owner->p_pictx->logFile))
    {
      fflush(owner->p_pictx->logFile);
    }

    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW", _disconnect, DONE"));
  }

  static gboolean _dw_init(LogThreadedDestWorker *s)
  {
    CrDestinationWorker *self = (CrDestinationWorker *)s;
    
    msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", CRMDW", _dw_init"));
    /*
    You can create thread specific resources here. In this example, we
    store the thread id.
    */
   
    self->thread_id = get_thread_id();
    self->file = NULL;
    
    gboolean ret = log_threaded_dest_worker_init_method(s);
    
    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW", _dw_init, DONE"), 
            evt_tag_long("ret", (gint)ret));

    return ret;
  }

  static void _dw_deinit(LogThreadedDestWorker *s)
  {
    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW",  _dw_deinit"));
    /*
    If you created resources during _thread_init, you need to free them here
    */
    
    log_threaded_dest_worker_deinit_method(s);
    
    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW", _dw_deinit, DONE"));
  }

  static void _dw_free(LogThreadedDestWorker *s)
  {
    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW",  _dw_free"));
    
    /*
    If you created resources during new, you need to free them here.
    */
    
    log_threaded_dest_worker_free_method(s);

    msg_info(CR_INFO_PREFIX, evt_tag_str("Reason", CRMDW",  _dw_free, DONE"));
  }

  LogThreadedDestWorker *cr_destination_dw_new(LogThreadedDestDriver *o, gint worker_index)
  {
    CrDestinationWorker *self = g_new0(CrDestinationWorker, 1);

    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW", cr_destination_dw_new"));

    log_threaded_dest_worker_init_instance(&self->super, o, worker_index);

    self->super.init = _dw_init;
    self->super.deinit = _dw_deinit;
    self->super.insert = _dw_insert;
    self->super.free_fn = _dw_free;
    self->super.connect = _connect;
    self->super.disconnect = _disconnect;

    msg_info(CR_INFO_PREFIX, 
            evt_tag_str("Reason", CRMDW", cr_destination_dw_new, DONE"));
    
    return &self->super;
  }

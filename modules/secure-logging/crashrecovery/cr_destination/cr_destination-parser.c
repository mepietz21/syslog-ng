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

/* File: cr_destination-parser.c */

#include "driver.h"
#include "cfg-parser.h"
#include "cr_destination-grammar.h"
#include "cr_destination-parser.h"

extern int cr_destination_debug;

int cr_destination_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword cr_destination_keywords[] =
{
  { "cr_destination", KW_CR_DESTINATION, KWS_NORMAL, "Destination crash recovery" },
  { "filenametoken", KW_CR_FILENAMETOKEN, KWS_NORMAL, "Log file name part (used for plain and enc)" },
  { "keypath", KW_CR_KEYPATH, KWS_NORMAL, "Crash recovery key path"  },
  { "dir", KW_CR_DIR, KWS_NORMAL, "Working directory crash recovery" },
  { "logrotcnt", KW_CR_LOGROTCNT, KWS_NORMAL, "Log rotation max lines" },
  { "mode", KW_CR_MODE, KWS_NORMAL, "Log modes: direct, enc, base64" },
  { 0 }
};


static void cr_destination_cleanup_wrapper(gpointer pipe)
{
  log_pipe_unref((LogPipe *)pipe);
}

CfgParser cr_destination_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &cr_destination_debug,
#endif
  .name = "cr_destination",
  .keywords = cr_destination_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) cr_destination_parse,
  .cleanup = cr_destination_cleanup_wrapper,
};

//-- This has been replaced: .cleanup = (void (*)(gpointer)) log_pipe_unref,
//   by the wrapper: .cleanup = cr_destination_cleanup_wrapper,
//-- Reason:
//   warning: cast between incompatible function types from ‘gboolean (*)(LogPipe *)’
//   {aka ‘int (*)(struct _LogPipe *)’} to ‘void (*)(void *)’ [-Wcast-function-type]

CFG_PARSER_IMPLEMENT_LEXER_BINDING(cr_destination_, CR_DESTINATION_, LogDriver **)

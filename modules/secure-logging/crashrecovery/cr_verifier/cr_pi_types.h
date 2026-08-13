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


#ifndef cr_pi_types_h
#define cr_pi_types_h

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "cr_pi_shared.h"
#include "cr_crypto.h"


/* namespce PI -> cr_pi to long --> just cr_ */
typedef struct _cr_VerifierContext
{
  /* std::string logFileDirectory;  directory holding the encrypted log file. */
  /* std::string outFile;  file path of the output log file, which will hold the readable logs. */
  /* std::string masterKeyPath;  master key path. */
  char logFileDirectory[PATH_MAX];
  char inEncFilePath[PATH_MAX];  /* encrypted log file (== output of Logger) */
  char outPlainFilePath[PATH_MAX]; /* decrypted plain log file (== output of Verifier) */
  char outProtocolPath[PATH_MAX + 10]; /* protocol (== output of Verifier) */
  char masterKeyPath[PATH_MAX];
  gsize n; /* max number of log entries. */
  gsize m; /* log file length. */
  gboolean useMetal; /* indicator to use GPU oder CPU */
  FILE *protocolFile; /* used in several files */
} cr_VerifierContext;

/* typedef std::array<unsigned char, KEY_SIZE> KEY_TYPE; */
typedef unsigned char cr_KEY_TYPE[KEY_SIZE];
typedef struct _cr_Keys
{
  cr_KEY_TYPE Key;
  cr_KEY_TYPE EncKey;
  cr_KEY_TYPE DrnKey;
  cr_KEY_TYPE TagKey;
  cr_KEY_TYPE IDKey;
} cr_Keys;


typedef struct _cr_KeyStoreEntry
{
  cr_Keys Ki;
  gint i;
  gint lj;
} cr_KeyStoreEntry;


/* define types for the requiered byte arrays */
/* typedef std::array<unsigned char, ID_LEN> ID_TYPE; */
/* typedef std::array<unsigned char, INTEGRITY_TAG_LEN> TAG_TYPE; */
/* typedef std::array<unsigned char, CIPHERTEXT_LEN> XOR_TYPE; */
/* typedef std::array<unsigned char, MESSAGE_LEN_SLOGCR> LOG_MESSAGE_TYPE; */

typedef unsigned char cr_ID_TYPE[ID_LEN] __attribute__((aligned(32)));
typedef unsigned char cr_TAG_TYPE[INTEGRITY_TAG_LEN] __attribute__((aligned(32)));
typedef unsigned char cr_XOR_TYPE[CIPHERTEXT_LEN] __attribute__((aligned(32)));
typedef unsigned char cr_LOG_MESSAGE_TYPE[MESSAGE_LEN_SLOGCR] __attribute__((aligned(32)));

/*  struct that holds one "line" of the log. */
typedef struct _cr_Tau_i
{
  cr_XOR_TYPE XOR;
  cr_TAG_TYPE TAG;  /* original name was just one letter: T */
  cr_ID_TYPE ID;
} __attribute__((aligned(32))) cr_Tau_i;


typedef int cr_Random[THE_K]; /* THE_K == K == 5 */

#endif /* cr_pi_types_h */


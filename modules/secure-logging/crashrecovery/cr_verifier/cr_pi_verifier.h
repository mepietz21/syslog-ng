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


#ifndef cr_pi_verifier_h
#define cr_pi_verifier_h

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "cr_result.h"
#include "cr_pi_types.h"



gboolean cr_check_cpu_cfg(void);
void cr_print_cpu_cfg(void);
GString *get_cpu_config_info(gboolean is_add_new_line);
cr_Result cr_Verify(cr_VerifierContext *ctx);
cr_Result cr_verifySingleLogFile(cr_VerifierContext *ctx);
gboolean cr_readMasterKey(const char *path, unsigned char key[KEY_SIZE]);
GString *cr_decryptLog(cr_KEY_TYPE key, cr_XOR_TYPE encLogMessage);

guint fnv1a_hash_ID_LEN(gconstpointer key);
void print_KeyStore_value_pair(gpointer keyVoid, gpointer valueVoid, gpointer user_data);
void destroy_value_KeyStoreEntry(gpointer value);
void destroy_key_KeyStore_ID_TYPE(gpointer keyVoid);

void cr_fill_struct_keys(cr_Keys *ks,
                         const unsigned char *Ki,
                         const unsigned char *encKey,
                         const unsigned char *drnKey,
                         const unsigned char *tagKey,
                         const unsigned char *idKey);

gboolean id_type_buffer_equal(gconstpointer a, gconstpointer b);
void destroy_value_drnbuffer(gpointer value);
void print_drns_value_pair(gpointer keyVoid, gpointer valueVoid, gpointer user_data);
gboolean is_equal_nullvector(unsigned char *buf, gsize len, gboolean debug);

#endif /* cr_pi_verifier_h */

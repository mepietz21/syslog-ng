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


#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "utils_slog.h"
#include "cr_pi_shared.h"
#include "cr_matrix.h"
#include "cr_pi_verifier.h"
#include "cr_plain_gauss_helper.h"


gboolean g_is_verbose = FALSE;



//----------------------------------------------------------------------
// cr_set_result
// Helper to set return code values
// in/out res Pointer to instance of cr_Result
// in code
// in success
// return -

void cr_set_result(cr_Result *res, int code, gboolean success)
{
  if (NULL == res)
    {
      // fixed from g_critical to msg_fatal
      msg_fatal("Invalid result pointer in cr_set_result");
      return;
    }
  res->code = code;
  res->success = success;
}



//----------------------------------------------------------------------
// cr_fill_struct_keys
// Helper to copy keys into a container struct
// The keys are of type fth_KEY_TYPE (unsignd char buffer of size KEY_SIZE)
// in/out ks of type fth_Keys
// in Ki
// in encKey
// in drnKey
// in tagKey
// in idKey
// return -
void cr_fill_struct_keys(cr_Keys *ks,
                         const unsigned char *Ki,
                         const unsigned char *encKey,
                         const unsigned char *drnKey,
                         const unsigned char *tagKey,
                         const unsigned char *idKey)
{
  memcpy(ks->Key, Ki, KEY_SIZE);
  memcpy(ks->EncKey, encKey, KEY_SIZE);
  memcpy(ks->DrnKey, drnKey, KEY_SIZE);
  memcpy(ks->TagKey, tagKey, KEY_SIZE);
  memcpy(ks->IDKey, idKey, KEY_SIZE);
}



//----------------------------------------------------------------------
// destroy_key_KeyStore_ID_TYPE
// Helper for clean up the hash table key at the end when the KeyStore is deleted
// in data: Pointer to value of type cr_ID_TYPE, that is freed here
// return -

void destroy_key_KeyStore_ID_TYPE(gpointer keyVoid)
{
  cr_ID_TYPE *key = (cr_ID_TYPE *) keyVoid;
  if (NULL != key)
    {
      g_free(key);
    }
}



//----------------------------------------------------------------------
// destroy_value_KeyStoreEntry
// Helper for clean up at the end when the KeyStore is deleted (GHashTable ght_KeyStore)
// in data: Pointer to value of type cr_KeyStoreEntry, that is freed here
// return -

void destroy_value_KeyStoreEntry(gpointer value)
{
  // The value pointer is passed here. Since we are allocating MyValue on the
  // heap, we need to free it.i
  cr_KeyStoreEntry *val = (cr_KeyStoreEntry *) value;
  if (NULL != val)
    {
      // g_print("vvvvv destroy_value_KeyStoreEntry, val->i: %d, val->lj: %d\n", val->i, val->lj);
      g_free(val);
    }
}



//----------------------------------------------------------------------
// print_KeyStore_value_pair
// A simple function to be used with g_hash_table_foreach for GHashTable ght_KeyStore
// It must match the GHFunc signature.
// in keyVoid: Pointer to key type of hash table (cr_ID_TYPE)
// in valueVoid: Pointer to value type of hash table (cr_KeyStoreEntry)
// in user_data: Pointer, unused
// return -

void print_KeyStore_value_pair(gpointer keyVoid, gpointer valueVoid, gpointer user_data)
{
  (void)user_data; // Unused parameter
  if ((NULL == keyVoid) || (NULL == valueVoid))
    {
      // fixed from g_critical to msg_fatal
      msg_fatal("Invalid pointer in print_KeyStore_value_pair");
      return;
    }

  cr_ID_TYPE *key = (cr_ID_TYPE *) keyVoid;
  cr_KeyStoreEntry *value = (cr_KeyStoreEntry *) valueVoid;
  g_print("print_KeyStore_value_pair, key: %p, value: %p\n", (void *)(key), (void *)(value));

  //-- key
  //dbg_hexdump("key cr_ID_TYPE", *key, sizeof(cr_ID_TYPE));

  //-- value
  g_print("print_KeyStore_value_pair, Value: {i: %d, lj: %d}\n", value->i, value->lj);
  //dbg_hexdump("value->Ki.Key", value->Ki.Key,    KEY_SIZE);
  //dbg_hexdump("value->Ki.EncKey", value->Ki.EncKey, KEY_SIZE);
  //dbg_hexdump("value->Ki.DrnKey", value->Ki.DrnKey, KEY_SIZE);
  //dbg_hexdump("value->Ki.TagKey", value->Ki.TagKey, KEY_SIZE);
  //dbg_hexdump("value->Ki.IDKey", value->Ki.IDKey,  KEY_SIZE);
}



//----------------------------------------------------------------------
// print_key_from_ght_KeyStore
//
// Print key from GHashTable ght_KeyStore
// in keyVoid: Void Pointer to key type of hash table (cr_ID_TYPE)
// in user_data: Pointer, unused
// return -

void print_key_from_ght_KeyStore(gpointer keyVoid, gpointer user_data)
{
  (void) user_data; // Unused parameter
  if (NULL == keyVoid)
    {
      // fixed from g_critical to msg_fatal
      msg_fatal("Invalid key pointer in print_key_from_ght_KeyStore");
      return;
    }

  // removed dependency to utils_slog.c
  //cr_ID_TYPE *key = (cr_ID_TYPE *) keyVoid;
  //-- key
  //dbg_hexdump("pKS key cr_ID_TYPE", *key, sizeof(cr_ID_TYPE));
}



//----------------------------------------------------------------------
// fnv1_hash
// Hash function unsed in context of GHashTable ght_KeyStore
// FNV-1a constants are defined in cr_pi_shared.h
// #define FNV_PRIME_32 16777619
// #define FNV_OFFSET_BASIS_32 2166136261U
// in key: Pointer to ID which is a buffer of type fth_ID_TYPE
// return 32-bit integer hash value
//
guint fnv1a_hash_ID_LEN(gconstpointer key)
{
  const unsigned char *data = (const unsigned char *)key;
  const int BufferSize = sizeof(cr_ID_TYPE); // ID_LEN  16
  guint hash = FNV_OFFSET_BASIS_32;
  for (int i = 0; i < BufferSize; ++i)
    {
      hash ^= data[i];
      hash *= FNV_PRIME_32;
    }
  return hash;
}



//----------------------------------------------------------------------
// id_buffer_equal
//
// Compares two buffers of type cr_ID_TYPE in context of GHashTable ght_KeyStore
// returns TRUE only when equal buffers else FALSE (FALSE also when invalid pointer detected)
//
gboolean id_type_buffer_equal(gconstpointer a, gconstpointer b)
{
  if ((NULL == a) || (NULL == b))
    {
      msg_fatal("Invalid pointer in id_type_buffer_equal");
      return FALSE;
    }
  return memcmp(a, b, ID_LEN) == 0;
}



//----------------------------------------------------------------------
// destroy_value_drnbuffer
// Helper for clean up at the end when the buffer of random numbers from DRNe is deleted (GHashTable ght_drns)
// in value: Pointer to value of type cr_Random, that is freed here
// return -
//
void destroy_value_drnbuffer(gpointer value)
{
  // The value pointer is passed here. Since we are allocating MyValue on the
  // heap, we need to free it.
  cr_Random *val = (cr_Random *) value;
  if (NULL != val)
    {
      free(val);
    }
}



//----------------------------------------------------------------------
// print_drns_value_pair
// A simple function to be used with g_hash_table_foreach for GHashTable ght_drns
// It must match the GHFunc signature.
// in keyVoid: Pointer to key type of hash table (int)
// in valueVoid: Pointer to value type of hash table (cr_Random)
// in user_data: Pointer, unused
// return -

void print_drns_value_pair(gpointer keyVoid, gpointer valueVoid, gpointer user_data)
{
  (void) user_data; //-- Unused parameter
  if ((NULL == keyVoid) || (NULL == valueVoid))
    {
      g_critical("failed: print_drns_value_pair with invalid pointer\n");
      return;
    }
  int *key = (int *) keyVoid;
  cr_Random *value = (cr_Random *) valueVoid;
  int *p = (int *) value;

  const int K2 = sizeof(cr_Random) / (sizeof(int));
  for (int r = 0; r < K2 ; ++r)
    {
      if (r == 0)
        {
          g_print("ght_drns entry Key: %d, { %d, ", *key, *p);
        }
      else
        {
          if (r != K2 - 1)
            g_print(" %d, ", *p);
          else
            g_print("%d }\n", *p);
        }
      p = p + 1;
    }
}



//----------------------------------------------------------------------
// cr_Verify
//
// Entry point to start the verification of log given by ctx provided
// in ctx
// returns cr_Result

cr_Result cr_Verify(cr_VerifierContext *ctx)
{
  // Old: For the time being: Limitation to just only one encrypted log file
  //      and the first found file in folder is processed.
  //      The original function aggregates all encrypted log files in a container, see
  //      std::vector<std::string> Verifier::getAllLogFiles(const std::string& directoryPath, const std::string& extension)
  //
  // New: ctx provides exactly one full file name for the encrypted output file.
  //      The path of this file name is expected to be valid. No 'mkdir -p ...' is done here.
  //      The extension is up to the caller. KISS Keep it safe and simple.


  cr_Result res;
  cr_set_result(&res, 0, FALSE);

  if (NULL == ctx)
    {
      msg_fatal("Invalid verifier context");
      return res;
    }

  if (ctx->protocolFile)
    {
      (void) fprintf(ctx->protocolFile, "%s", "Enter cr_verify\n");
    }

  //-- HERE THE FULL WORK IS DONE ---
  //
  //
  res = cr_verifySingleLogFile(ctx);
  //
  //
  g_print("cr_Verify, cr_verifySingleLogFile returns res.code: %d, res.success: %d\n", res.code, (int) res.success);
  return res;
}



//----------------------------------------------------------------------
// cr_verifySingleLogFile
//
// Here to real work is done for verification of one log file
//
// in ctxi: Context providing parsed arguemnts
//
// returns cr_Result On success: code == 1, success == TRUE
//                    On Error:   code == 0, sussess == FALSE

cr_Result cr_verifySingleLogFile(cr_VerifierContext *ctx)
{
  cr_Result res;
  cr_set_result(&res, 1, TRUE);  //-- Result res {1, true};
  if ((NULL == ctx) || (NULL == ctx->protocolFile))
    {

      msg_warning("Invalid verifier context or protocol file");
      cr_set_result(&res, 0, FALSE);
      return res;
    }
  char szBuffer[256]; //-- general purpose string buffer for log, timestamp etc
  memset(szBuffer, 0, sizeof(szBuffer));

  //(void) fprintf(ctx->protocolFile, "Enter cr_verifySingleLogFile\n");
  (void) fprintf(ctx->protocolFile, "%s", "Enter cr_verifySingleLogFile\n");

  //-- due to clean up when error: early declaration
  FILE *resultLogFile = NULL;
  GPtrArray *gpa_v = NULL;
  cr_XOR_TYPE *data_block = NULL;
  GHashTable *ght_drns = NULL;
  GHashTable *ght_KeyStore = NULL;
  GArray *garr_keys = NULL;
  GArray *garr_Tau = NULL;
  GPtrArray *gpa_c = NULL;
  struct cr_BMatrixType *Mat = NULL;

  //-- encrypted input file
  FILE *logFileEnc; //-- std::ifstream logFile(path, std::ios::binary); // this is our log file :*
  if ((logFileEnc = fopen(ctx->inEncFilePath, "rb")) == NULL) //-- if (!logFile.is_open()) {
    {
      msg_error("Failed to open input log file", evt_tag_str("path", ctx->inEncFilePath));
      cr_set_result(&res, 0, FALSE);
      return res;
    }

  cr_KEY_TYPE Ki, encKey, drnKey, tagKey, idKey, k0;

  //-- Hash table
  // Key: int
  // Value: buffer of THE_K int's (DRN Random) of type cr_Random

  ght_drns = g_hash_table_new_full( //-- std::unordered_map<int, array<int, K>> drns;
               g_direct_hash,
               g_direct_equal,
               NULL,
               destroy_value_drnbuffer);

  //-- Hash table
  //-- std::unordered_map<ID_TYPE, KeyStoreEntry> KeyStore;
  // key: cr_ID_TYPE is: typedef unsigned char cr_ID_TYPE[ID_LEN];
  //      cr_ID_TYPE is filled later by int CreateID(unsigned char *key, int j, unsigned char IDlj[ID_LEN])
  // value: cr_KeyStoreEntry (struct)
  ght_KeyStore = g_hash_table_new_full(
                   fnv1a_hash_ID_LEN, //-- hash function
                   id_type_buffer_equal, //-- compare IDs
                   destroy_key_KeyStore_ID_TYPE,  // The destroy function for our key buffer
                   destroy_value_KeyStoreEntry);  // The destroy function for our value struct

  //-- vector<Keys> keys; loop ctx->n
  garr_keys = g_array_sized_new(TRUE, TRUE, sizeof(cr_Keys), (guint) ctx->n);
  //g_print("garr_keys: ctx->n: %d, sizeof(cr_Keys): %d\n", ctx->n, sizeof(cr_Keys) );

  //-- preallocate ctx-m elements,  std::vector<Tau_i> Tau(ctx->m);
  garr_Tau = g_array_sized_new(TRUE, TRUE, sizeof(cr_Tau_i), (guint) ctx->m);
  g_array_set_size(garr_Tau, (guint)
                   ctx->m); //-- do not use append, instead: g_array_index(garr_Tau, cr_Tau_i, i) = taui;
  //g_print("garr_Tau: ctx->m: %d, sizeof(cr_Tau_i): %d, garr_Tau->len: %d\n", ctx->m, sizeof(cr_Tau_i),
  //        garr_Tau->len);

  //const cr_XOR_TYPE nullVector = {0}; //-- const XOR_TYPE nullVector = {0};
  //g_print("sizeof(cr_XOR_TYPE): %d, CIPHERTEXT_LEN: %d\n", sizeof(cr_XOR_TYPE), CIPHERTEXT_LEN);

  int rank = 0; // line 10

  gboolean is_key = cr_readMasterKey(ctx->masterKeyPath, k0);
  if (FALSE == is_key)
    {
      msg_error("Failed to read master key", 
                evt_tag_str("path", ctx->masterKeyPath));
      cr_set_result(&res, 0, FALSE);
      (void) fprintf(ctx->protocolFile, "ERROR: Failed to read master key %s\n", ctx->masterKeyPath);
      goto LABEL_CLEANUP;
    }
  memcpy(Ki, k0, KEY_SIZE); //-- Ki = k0

  // generate all possible n keys
  // line 1
  for (int i = 1; i <= ctx->n; ++i) //-- Note: verified: ctx->n
    {
      cr_Random kRandom; //-- std::array<int, K> kRandom;
      // Fill the array with -1
      // thats why the upper bound cant be larger than int_max
      int cnt_random = G_N_ELEMENTS(kRandom);
      for (int r = 0; r < cnt_random; ++r)
        {
          kRandom[r] = -1;
        }
      // generate the ith keye.
      // line 2
      if (0 == cr_KeyEvolution(Ki, Ki))
        {
          msg_error("Key evolution failed");
          cr_set_result(&res, 0, FALSE);
          (void) fprintf(ctx->protocolFile, "%s", "ERROR: Failed key evolution\n");
          goto LABEL_CLEANUP;
        }
      // derive all sub keys, and store them.
      if (0 == cr_DeriveSubKeys(Ki, encKey, drnKey, tagKey, idKey))
        {
          msg_error("Failed to derive sub keys");
          cr_set_result(&res, 0, FALSE);
          (void) fprintf(ctx->protocolFile, "%s", "ERROR: Failed derive sub keys\n");
          goto LABEL_CLEANUP;
        }

      cr_Keys ks;
      cr_fill_struct_keys(&ks, Ki, encKey, drnKey, tagKey, idKey); //-- Keys ks = {Ki, encKey, drnKey, tagKey, idKey};
      g_array_append_vals(garr_keys, &ks, 1); //-- keys.push_back(ks)

      // re generate the k distinct random locations.
      // line 3
      //g_print("cr_DRN, line 3\n");
      if (0 == cr_DRN(drnKey, THE_K, ctx->m, kRandom)) //-- Note: verified: ctx->m
        {
          msg_error("Failed to create distinct random locations");
          cr_set_result(&res, 0, FALSE);
          (void) fprintf(ctx->protocolFile, "%s", "ERROR: Failed to create k distinct random numbers\n");
          goto LABEL_CLEANUP;
        }
      //-- copy randoms and store them into hash table ght_drns ---
      cr_Random *p_drnsvalue = (cr_Random *) g_malloc0(sizeof(cr_Random));
      memcpy(p_drnsvalue, kRandom, sizeof(cr_Random));

      g_hash_table_insert(ght_drns, GINT_TO_POINTER(i - 1), p_drnsvalue); //-- drns[(i-1)] = kRandom;

      // regenerate all key IDs for each of the k locations.
      // line 4
      for (int j = 0; j < THE_K; ++j)
        {
          cr_ID_TYPE *p_ID = (cr_ID_TYPE *) g_malloc0(sizeof(cr_ID_TYPE)); //-- array<unsigned char, ID_LEN> ID;
          int lj = kRandom[j];

          // generate the ID.
          // line 5
          //g_print("cr_CreateID, line 5\n");
          if (0 == cr_CreateID(idKey, j, *p_ID))
            {
              msg_error("Failed to create ID", 
                        evt_tag_int("key_index", j));
              cr_set_result(&res, 0, FALSE);
              (void) fprintf(ctx->protocolFile, "%s", "ERROR: Failed to create ID\n");
              goto LABEL_CLEANUP;
            }
          // store the k IDs with the associated key, log iteration index, and the respective location within the log file.
          //-- KeyStoreEntry newEntry = {ks, i, lj};
          //-- KeyStore[ID] = newEntry;
          cr_KeyStoreEntry *p_kse = (cr_KeyStoreEntry *) g_malloc0(sizeof(cr_KeyStoreEntry));
          p_kse->Ki = ks;
          p_kse->i = i;
          p_kse->lj = lj;
          g_hash_table_insert(ght_KeyStore, p_ID, p_kse);
          //print_KeyStore_value_pair(p_ID, p_kse, NULL);
          //g_print("g_hash_table_insert(ght_KeyStore, p_ID, p_kse); done\n\n");
        } //-- for line 4
    } //-- for line 1

  // Predict M's rank:
  //-- std::array<unsigned char, LOG_LEN>log;//(ID_LEN);
  guchar entry[LOG_LEN];
  //g_print("\nREAD FILE\n");
  //int j;
  // line 11
  for (int i = 0; i < ctx->m; ++i)
    {
      // get the entry Tau_i from the log file.
      //-- logFile.seekg(i * LOG_LEN, std::ios::beg);
      //   logFile.read(reinterpret_cast<char*>(log.data()), LOG_LEN);
      //   if (!logFile) {
      //       std::cerr << "Error: reading from log file. Consider choosing right amount for N." << std::endl;
      //       exit(EXIT_FAILURE);
      //   }

      // Use off_t for offsets to support large files
      off_t offset = (off_t)i * LOG_LEN;
      // Use fseeko, the large-file-aware version of fseek
      if (fseeko(logFileEnc, offset, SEEK_SET) != 0)
        {
          msg_error("Failed to seek in encrypted input log file", 
                    evt_tag_str("path", ctx->inEncFilePath), 
                    evt_tag_int("entry", i), 
                    evt_tag_int64("offset", (gint64)offset));

          (void) fprintf(ctx->protocolFile, 
                        "%s", 
                        "WARNING: Failed to read from encrypted input file\n");
          break; // Stop processing further entries
        }

      //-- logFile.read(reinterpret_cast<char*>(log.data()), LOG_LEN);
      gsize bytes_read = fread(entry, sizeof(guchar), LOG_LEN, logFileEnc);
      if (bytes_read < LOG_LEN)
        {
          if (feof(logFileEnc))
            {
              msg_warning("Unexpected end of encrypted input log file", 
                          evt_tag_str("path", ctx->inEncFilePath), 
                          evt_tag_int("entry", i), 
                          evt_tag_int("bytes_read", (gint)bytes_read), 
                          evt_tag_int("expected_bytes", LOG_LEN));
              (void) fprintf(ctx->protocolFile, "WARNING: Reached end of file unexpectedly while reading entry %d.\n", i);
            }
          else if (ferror(logFileEnc)) 
            { 
              msg_error("Failed to read encrypted input log file", 
                        evt_tag_str("path", ctx->inEncFilePath), 
                        evt_tag_int("entry", i)); 
                
              (void) fprintf(ctx->protocolFile, "ERROR: File read error occurred while reading " "entry %d: %s.\n", i, g_strerror(errno)); 
            }
          break; //-- Stop processing further entries
        }
      // parse the entry Tau_i
      //-- Tau_i taui;
      //   std::copy(log.begin(), log.begin() + CIPHERTEXT_LEN, taui.XOR.begin());
      //   std::copy(log.begin() + CIPHERTEXT_LEN, log.begin() + CIPHERTEXT_LEN + INTEGRITY_TAG_LEN, taui.T.begin());
      //   std::copy(log.begin() + CIPHERTEXT_LEN + INTEGRITY_TAG_LEN, log.begin() + CIPHERTEXT_LEN + INTEGRITY_TAG_LEN + ID_LEN,
      //          taui.ID.begin());
      //   Tau[i] = taui;
      //g_print("loop i: %d, fseekl offset: %lld, read LOG_LEN %d\n", i, (long long)offset, LOG_LEN);

      cr_Tau_i tau_i;
      memset(&tau_i, 0, sizeof(cr_Tau_i));
      memcpy(tau_i.XOR, entry, CIPHERTEXT_LEN);
      memcpy(tau_i.TAG, entry + CIPHERTEXT_LEN, INTEGRITY_TAG_LEN);
      memcpy(tau_i.ID, entry + CIPHERTEXT_LEN + INTEGRITY_TAG_LEN, ID_LEN);
      if (TRUE == is_equal_nullvector(tau_i.ID, ID_LEN, TRUE))
        {
          g_print("taui.ID is zero, i: %d\n\n", i);
          (void) fprintf(ctx->protocolFile, "taui.ID is zero, loop index %d:\n", i);
        }

      //-- An empty entry already has been prepared (by g_array_set_size), so do NOT use g_array_append_val(garr_Tau, tau_i);
      g_array_index(garr_Tau, cr_Tau_i, i) = tau_i;
      //-- Check done. Buffers are found in Logger log also. So file is read
      //   correctly by Verifier

      // check if the ID can be found in the KeyStore, and if it is found, check wether it is the current highest number.
      // line 12
      //--  auto it = KeyStore.find(taui.ID);
      //    if (KeyStore.end() == it) {
      //      continue; // NEXT
      //    }
      gboolean is_found = g_hash_table_contains(ght_KeyStore, tau_i.ID);
      if (FALSE == is_found)
        {
          // dbg_hexdump((unsigned char*) "tau_i.ID not found in ght_KeyStore", tau_i.ID, ID_LEN);
          // (void) fprintf(ctx->protocolFile, "taui.ID not found in key store, loop index %d:\n", i);
          continue; // NEXT
        }
      const cr_KeyStoreEntry *p_kse = g_hash_table_lookup(ght_KeyStore, tau_i.ID);
      if (NULL == p_kse)
        {
          // dbg_hexdump((unsigned char*) "tau_i.ID is in ght_KeyStore but p_kse is NULL", tau_i.ID, ID_LEN);
          // (void) fprintf(ctx->protocolFile, "taui.ID is in key store but p_kse is NULL, loop index %d:\n", i);
          continue; // NEXT
        }

      //-- j = it->second.i;
      int j_11 = p_kse->i;
      // line 13
      //-- rank = max(rank, j);
      rank = MAX(rank, j_11);
    } //-- for line 11

  // check if rank is larger 0
  // line 15
  if (rank == 0)
    {
      char szErrorMsg[] =
        "ERROR - Wrong key? Algorithm 3: ListItems(DS,K0,n), line 12, \"if rank = 0 then output \xE2\x8A\xA5\"\n"; //-- up tack: E2 8A A5
      g_print("%s", szErrorMsg);

      msg_error("Verification failed: rank is zero", 
                evt_tag_str("reason", "wrong key or invalid/tampered log"));

      cr_set_result(&res, 0, FALSE);
      (void) fprintf(ctx->protocolFile, "%s", szErrorMsg);
      goto LABEL_CLEANUP;
    }



  //-- cout << "Detected " << rank << " different log entries." << endl;
  g_print("Detected %d different log entries, line 9\n", rank);

  // Create M=m x n zero Matrix over GF(2).
  // line 9
  //-- M = new BMatrixType(ctx->m, rank);
  //struct cr_BMatrixType *Mat = cr_BMatrix_ctor_dyn(ctx->m, rank);
  Mat = cr_BMatrix_ctor_dyn(ctx->m, rank);
  cr_KeyStoreEntry kse_temp_not_in_KeyStore;

  // null all vectors in the log file, which have been tampered, to avoid them corrupting the output.
  // line 16
  for (int i = 0; i < rank; ++i)
    {
      // line 18
      for (int j = 0; j < THE_K; ++j)
        {
          // get the k distinct random locations, for the ith log iteration.
          // line 17
          //-- int lj = drns[i][j];
          cr_Random *p_drnsvalue = g_hash_table_lookup(ght_drns, GINT_TO_POINTER(i));
          if (NULL == p_drnsvalue)
            {
              msg_error("Missing random locations for log iteration", 
                        evt_tag_int("iteration", i));

              cr_set_result(&res, 0, FALSE);
              (void) fprintf(ctx->protocolFile, "ERROR: g_hash_table_lookup for %d returns value NULL\n", i);
              goto LABEL_CLEANUP;
            }
          guint lj = (guint) (*p_drnsvalue)[j];
          //g_print("i: %d, j: %d, lj = (*p_drnsvalue)[j]: %d\n", i, j, lj);

          // line 18
          //-- KeyStoreEntry kse = KeyStore[Tau[lj].ID];
          if (lj >= garr_Tau->len)
            {
              // fixed: Out-of-Bounds + NULL Pointer were possible
              // old: g_print("check index lj-1, line 18, Tau[lj].ID, out of range, ...");

              if (lj >= garr_Tau->len) { 
                msg_error("Random location is outside log range", 
                          evt_tag_uint("location", lj), 
                          evt_tag_uint("log_entries", garr_Tau->len), 
                          evt_tag_int("iteration", i), 
                          evt_tag_int("random_index", j)); 
                  
              (void) fprintf(ctx->protocolFile, 
                            "ERROR: Tau index out of range, " 
                            "lj: %u, garr_Tau->len: %u\n", 
                            lj, 
                            garr_Tau->len); 
                
              cr_set_result(&res, 0, FALSE); 
              goto LABEL_CLEANUP; 

              }
            }

          cr_Tau_i tau_lj = g_array_index(garr_Tau, cr_Tau_i, lj);
          //-- Node: The returned tau_lj might be empty, if so, kse has to be created!
          // gboolean is_empty_tau_lj = is_equal_nullvector(tau_lj.ID, sizeof(cr_ID_TYPE), TRUE);
          //-- in c++ a new empty entry is generated when accessing none existing map entry!  KeyStoreEntry kse = KeyStore[Tau[lj].ID];
          cr_KeyStoreEntry *p_kse = g_hash_table_lookup(ght_KeyStore, tau_lj.ID);
          if (NULL == p_kse)
            {
              msg_warning("Log entry references unknown key ID", 
                          evt_tag_int("iteration", i), 
                          evt_tag_int("location", j));

              // This can happen when log file provides less lines then ctx->n
              //p_kse = (cr_KeyStoreEntry *) g_malloc0(sizeof(cr_KeyStoreEntry));
              // above cr_KeyStoreEntry kse_temp_not_in_KeyStore;
              memset(&kse_temp_not_in_KeyStore, 0, sizeof(cr_KeyStoreEntry));
              p_kse = &kse_temp_not_in_KeyStore;
              cr_set_result(&res, 0, FALSE);
              //-- DO NOT exit here. Can happen when tampered.
              (void) fprintf(ctx->protocolFile, "WARNING: ID which should not exist! Tampered log file? i: %d, j: %d\n", i, j);
            }
          //--  TAG_TYPE _T;
          cr_TAG_TYPE _TAG;
          memset(&_TAG, 0, sizeof(cr_TAG_TYPE));

          // create the integrity tag based on the XOR part
          // line 20
          //-- if (0 == CreateIntegrityTag(kse.Ki.TagKey.data(), Tau[lj].XOR.data(), _T.data())) {
          //        cerr << "ERROR: Failed to create the integrity tag." << endl;
          //        exit(EXIT_FAILURE);
          //    }

          if (0 == cr_CreateIntegrityTag(p_kse->Ki.TagKey, tau_lj.XOR, _TAG))
            {
              msg_error("Failed to create integrity tag", 
                        evt_tag_int("iteration", i), 
                        evt_tag_int("location", j));
              cr_set_result(&res, 0, FALSE);
              (void) fprintf(ctx->protocolFile, "%s", "ERROR: Failed to create the integrity tag\n");
              goto LABEL_CLEANUP;
            }

          // check whether the vector has been tampered
          // line 20
          //-- if (kse.lj == lj && _T == Tau[lj].T) {
          //      // toggle the bit in M
          //      // line 20
          //      M->setBit(lj, i);// data[lj * M->buckets + i] = 1;
          //      continue;
          //}

          gboolean is_equal_TAG = FALSE;
          if (0 == memcmp(_TAG, tau_lj.TAG, sizeof(cr_TAG_TYPE)))
            {
              is_equal_TAG = TRUE;
            }
          if ((guint)(p_kse->lj) == lj && is_equal_TAG)
            {
              // toggle the bit in M
              // line 20
              cr_BMatrix_setBit(Mat, lj, i); // data[lj * M->buckets + i] = 1;
              continue;
            }

          // check if it has been nulled already, because the locations could be check severall times.
          //-- if (Tau[lj].XOR != nullVector) {
          //      cout << "Line '" << lj << "' has been tampered." << endl;
          //   }

          if (FALSE == is_equal_nullvector(tau_lj.XOR, sizeof(cr_XOR_TYPE), TRUE))
            {
              // fixed, old: g_print
              msg_warning("Log line has been tampered", 
                          evt_tag_uint("line", lj));

              (void) fprintf(ctx->protocolFile, "INFO: Line %u has been tampered!\n", lj);
            }

          // null the tampered vector.
          // line 21
          //-- std::fill(Tau[lj].XOR.begin(), Tau[lj].XOR.end(), 0);
          memset(tau_lj.XOR, 0, sizeof(cr_XOR_TYPE));
          // set basic tampering indicator
          cr_set_result(&res, 0, FALSE);

        } //-- for line 18
    } //-- for line 16




  // Remove the random PAD.
  // line 23
  //-- PRGContext *prgCtx = CreatePRGContext(k0.data());
  //-- std::array<unsigned char, LOG_LEN> randomPad;
  // see above: cr_KEY_TYPE Ki, encKey, drnKey, tagKey, idKey, k0;
  cr_PRGContext *prgCtx = cr_CreatePRGContext(k0);
  if (NULL == prgCtx)
    {
      msg_error("Failed to create PRG context");
      cr_set_result(&res, 0, FALSE);
      (void) fprintf(ctx->protocolFile, "%s", "ERROR: prgCtx is NULL!\n");
      goto LABEL_CLEANUP;
    }
  unsigned char randomPad[LOG_LEN] __attribute__((aligned(32)));
  memset(randomPad, 0, LOG_LEN);

  // line 24
  for (int i = 0; i < ctx->m; ++i)
    {
      // line 25
      // int cr_PRG(cr_PRGContext *ctx, unsigned char *buffer, int size)
      //-- if (-1 == PRG(prgCtx, randomPad.data(), LOG_LEN)) {
      if (-1 == cr_PRG(prgCtx, randomPad, LOG_LEN))
        {
          g_free(prgCtx);
          prgCtx = NULL;
          msg_error("Failed to create random pad");
          cr_set_result(&res, 0, FALSE);
          (void) fprintf(ctx->protocolFile, 
                        "%s", 
                        "ERROR: Failed to creatd random PAD!\n");

          goto LABEL_CLEANUP;
        }
      // It is important to generate the PRG output first, and abort the iteration after, otherwise the internal state (counter) differs from the original state.

      // Check if the XOR part has been nulled.
      // line 25

      cr_Tau_i tau_i = g_array_index(garr_Tau, cr_Tau_i, i);

      //-- if (Tau[i].XOR == nullVector)
      if (TRUE == is_equal_nullvector(tau_i.XOR, sizeof(cr_XOR_TYPE), TRUE))
        continue;

      // remove random pad of the XOR part of Tau[i].
      // line 25
      //--  std::transform(Tau[i].XOR.begin(), Tau[i].XOR.end(), randomPad.begin(), Tau[i].XOR.begin(), [](unsigned char a, unsigned char b){
      //     return a ^ b;
      //    });
      //

      /* working version not optimized
      for (int n = 0; n < CIPHERTEXT_LEN; ++n)
        {
          tau_i.XOR[n] = tau_i.XOR[n] ^ randomPad[n];
        }
      */
      xor_buffers(tau_i.XOR, randomPad, CIPHERTEXT_LEN);

      //-- value type, no pointer -> write back XOR manipulated!
      g_array_index(garr_Tau, cr_Tau_i, i) = tau_i;

    } //-- for line 24


  //-- delete prgCtx;
  g_free(prgCtx);
  prgCtx = NULL;

  //-- std::vector<XOR_TYPE> v;
  //-- v.reserve(Tau.size());
  // Use std::transform to extract XOR elements into a new vector
  //--    std::transform(Tau.begin(), Tau.end(), std::back_inserter(v), [](const Tau_i& tau) { return tau.XOR; });

  //-- prepare Gauss ---
  guint v_length = garr_Tau->len;
  //GPtrArray *gpa_v = g_ptr_array_new();
  gpa_v = g_ptr_array_new();
  g_ptr_array_set_size(gpa_v, v_length);
  gsize total_size = v_length * sizeof(cr_XOR_TYPE);
  //cr_XOR_TYPE *data_block = (cr_XOR_TYPE *)aligned_alloc(AVX2_ALIGNMENT, total_size);
  data_block = (cr_XOR_TYPE *)aligned_alloc(AVX2_ALIGNMENT, total_size);
  if (NULL == data_block)
    {
      msg_error("Failed to create aligned data block", 
                evt_tag_uint("size", total_size));
      cr_set_result(&res, 0, FALSE);
      (void) fprintf(ctx->protocolFile, "%s", "ERROR: Failed to create aligned data block for gpa_v!\n");
      goto LABEL_CLEANUP;
    }
#if 0
  memset(data_block, 0, total_size);
  for (guint i = 0; i < v_length; i++)
    {
      cr_Tau_i *p_tau_i = &g_array_index(garr_Tau, cr_Tau_i, i);
      cr_XOR_TYPE *p_xor_i = &data_block[i];
      memcpy(p_xor_i, p_tau_i->XOR, sizeof(cr_XOR_TYPE));
      gpa_v->pdata[i] = p_xor_i;
    } i
#endif
  const cr_Tau_i *tau_data = (cr_Tau_i *)garr_Tau->data;
  for (guint i = 0; i < v_length; i++)
    {
      cr_XOR_TYPE *p_dest = &data_block[i];
      memcpy(p_dest, tau_data[i].XOR, sizeof(cr_XOR_TYPE));
      gpa_v->pdata[i] = p_dest;
    }

  //
  //
  //-- GAUSS
  //
  //
  g_print("-- GAUSS -----\n");
  get_human_timestamp(szBuffer);
  g_print("%s\n", szBuffer);
  // GPtrArray *gpa_c = NULL;

  //-- std::vector<PI::XOR_TYPE> c;
  // choose metal or CPU:
  if (ctx->useMetal)
    {
      msg_warning("Metal is not supported; using CPU implementation");
      //-- ignore wrongly set flag
    }


  //-- auto t1 = high_resolution_clock::now();
  // solve gauss, and get the cipher text vector c

  // line 27 Verify, == line 22 listitems
  //--  c = PlainGaussHelper::Solve(M, v, false);
  gboolean debug = FALSE;
  gpa_c = cr_pgh_Solve(Mat, gpa_v, debug, ctx); //-- add ctx for protocol log
  get_human_timestamp(szBuffer);
  g_print("%s\n", szBuffer);

  //-- auto t2 = high_resolution_clock::now();
  //--          duration<long, std::nano> ns_double = t2 - t1;
  //--       cout << "Gaussian Elimination (CPU): " << ns_double.count() << " ns." << endl;
  //--    std::ofstream resultLogFile(resultPath, std::ios::app);
  //   // Check if the file is successfully opened
  //   if (!resultLogFile.is_open()) {
  //       std::cerr << "ERROR: opening the result file!" << std::endl;
  //       exit(EXIT_FAILURE);
  //   }

  // above FILE *resultLogFile;
  g_print("ctx.outFile: %s\n", ctx->outPlainFilePath);
  resultLogFile = fopen(ctx->outPlainFilePath, "w");
  if (NULL == resultLogFile)
    {
      msg_error("Failed to create output log file", 
                evt_tag_str("path", ctx->outPlainFilePath));
      cr_set_result(&res, 0, FALSE);
      (void) fprintf(ctx->protocolFile, "ERROR: Failed to create output file %s\n", ctx->outPlainFilePath);
      goto LABEL_CLEANUP;
    }

  // decrypt the log files, and check the MACs.
  // line 27
  gboolean is_show_decrypted = FALSE; //-- verbose terminal output
  g_print("line 27, rank: %d\n", rank);
  g_print("gpa_c->len: %d\n", gpa_c->len);
  for (int i = 0; i < rank; ++i)
    {
      //g_print("\n-- Before cr_decryptLog, i: %d ---\n", i);
      // decrypt the log message
      //line 29
      //-- string log = decryptLog(keys[i].EncKey, c[i]);
      cr_Keys keys_i = g_array_index(garr_keys, cr_Keys, i);
      cr_XOR_TYPE *p_xor_i  = g_ptr_array_index(gpa_c, i);

      //-- DECRYPT
      GString *gslog = cr_decryptLog(keys_i.EncKey, *p_xor_i);

      //-- add extra \n for better readablility in terminal. gslog->str should
      //   already contain a '\n' due the logger does not remove it.
      //glong cnt_of_char = g_utf8_strlen(gslog->str, -1);

      if (TRUE == is_show_decrypted)
        {
          g_print("-- %d of %d:\n", i + 1, rank);
          cr_print_gstring_info(gslog, "decrypted log line", TRUE, TRUE);
        }
      else
        {
          if ( (0 == (i & 511)) || (i + 1 == rank))
            g_print("\x1b[2K\r   decrypted log line %d of %d\n", i + 1, rank);
        }


      // check wether the log message has been tampered.
      // line 30
      //

      //--  if (log == "") {
      //       continue;
      //    }
      if (0 == gslog->len)
        {
          g_print("gslog->len == 0, empty string, i: %d, line 27/30\n", i);
          continue;
        }

      // write the log into the desired file.
      //-- resultLogFile << log << endl;
      fprintf(resultLogFile, "%s", gslog->str);

      g_string_free(gslog, TRUE);
      gslog = NULL;

    } // for i line 27
  g_print("\n");
  get_human_timestamp(szBuffer);
  g_print("%s\n", szBuffer);



LABEL_CLEANUP:
  g_print("\n-- Clean up ---\n");

  //-- Clean up GArray
  //g_print("Clean up 1\n");
  if (NULL != garr_keys)
    {
      g_array_free(garr_keys, TRUE);
    }

  //g_print("Clean up 2\n");
  if (NULL != garr_Tau)
    {
      g_array_free(garr_Tau, TRUE);
    }

  //-- Free the entire data block with a single call.
  //g_print("Clean up 3\n");
  if (NULL != data_block)
    {
      free(data_block);
    }

  //g_print("Clean up 4\n");
  if (NULL != gpa_v)
    {
      g_ptr_array_free(gpa_v, TRUE);
    }

  //g_print("Clean up 5\n");
  //if (NULL != gpa_v) fixed
  if (NULL != gpa_c)
    {
      free_GPtrArray_cr_XOR_TYPE(&gpa_c);
    }
  //g_print("Clean up 6\n");
  if (NULL != ght_KeyStore)
    {
      g_hash_table_destroy(ght_KeyStore);
    }

  //g_print("Clean up 7\n");
  if (NULL != ght_drns)
    {
      g_hash_table_destroy(ght_drns);
    }

  //g_print("Clean up 8\n");
  if (NULL != Mat)
    {
      cr_BMatrix_destructor_dyn(&Mat);
    }

  if (NULL != logFileEnc)
    {
      fclose(logFileEnc); //-- close encrypted binary input log file
      logFileEnc = NULL;
    }

  if (NULL != resultLogFile)
    {
      fclose(resultLogFile); //-- close result output file
      resultLogFile = NULL;
    }

  //-- Note: the ctx->protocolFile is handled by caller in cr_verifier.c
  return res;
}



//----------------------------------------------------------------------
// cr_decryptLog
//
// Decrypts the provided log message
// in p_key:
// in p_encLogMessage
// returns instance of GString. The user is responsible for calling g_string_free(callerstring, TRUE);

GString *cr_decryptLog(cr_KEY_TYPE key, cr_XOR_TYPE encLogMessage)
{
  unsigned char iv[IV_SIZE];
  unsigned char ciphertext[MESSAGE_LEN_SLOGCR];
  unsigned char mac[MAC_LEN];
  unsigned char referenceMac[MAC_LEN];
  gsize len;
  gsize macLen;

  GString *gslog = g_string_new("");
  memset(referenceMac, 0, MAC_LEN);
  memset(ciphertext, 0, MESSAGE_LEN_SLOGCR);

  /* copy_n(encLogMessage.begin(), IV_SIZE, iv.begin()); */
  /* copy_n(encLogMessage.begin() + IV_SIZE, MESSAGE_LEN_SLOGCR, ciphertext.begin()); */
  /* copy_n(encLogMessage.begin() + IV_SIZE + MESSAGE_LEN_SLOGCR, MAC_LEN, mac.begin()); */

  memcpy(iv, encLogMessage, IV_SIZE);
  memcpy(ciphertext, encLogMessage + IV_SIZE, MESSAGE_LEN_SLOGCR);
  memcpy(mac, encLogMessage + IV_SIZE + MESSAGE_LEN_SLOGCR, MAC_LEN);

  /* CMAC(key.data(), ciphertext.data(), MESSAGE_LEN_SLOGCR, referenceMAC.data(), &macLen, MAC_LEN); */
  if ( ! cr_CMAC(key, ciphertext, MESSAGE_LEN_SLOGCR, referenceMac, &macLen, MAC_LEN))
    {
      msg_error("ERROR: cr_decryptLog, cr_CMAC fails!\n");
    }

  if (MAC_LEN != macLen)
    {
      msg_error("Failed: cr_decryptLog, Creating MAC failed!\n");
      //exit(EXIT_FAILURE);
    }

  if (0 != memcmp(mac, referenceMac, MAC_LEN))
    {
      msg_error("Invalid MAC detected!\n");
      return gslog;
    }

  /* ciphertext and plaintext: Both are the same length which is MESSAGE_LEN_SLOGCR (inclusive terminating zero when interpreted as C string) */
  unsigned char logm[MESSAGE_LEN_SLOGCR + 1];
  memset(logm, 0, MESSAGE_LEN_SLOGCR + 1);
  len = cr_AES_256_CTR_decrypt(ciphertext, MESSAGE_LEN_SLOGCR, key, iv, logm);
  if (MESSAGE_LEN_SLOGCR != len)
    {
      msg_error("Log decryption failed", 
                evt_tag_int("expected_length", MESSAGE_LEN_SLOGCR), 
                evt_tag_int("actual_length", len));
      return gslog; // fixed: function should return an empty GString on failure
    }
  g_string_append(gslog, (const gchar *) logm);
  return gslog;
}



//----------------------------------------------------------------------
// cr_readMasterKey
//
// in path: full filename to master key
// int key: key buffer
//
// retuns TRUE on SUCCESS and FALSE on FAILURE

gboolean cr_readMasterKey(const char *path, unsigned char key[KEY_SIZE])
{
  if (NULL == path)
    {
      msg_error("Failed to read master key", 
                evt_tag_str("reason", "invalid path"));
      return FALSE;
    }
  char szPath[PATH_MAX];
  strncpy(szPath, path, PATH_MAX);
  szPath[PATH_MAX - 1] = '\0';
  FILE *file = fopen(szPath, "rb");
  if (!file)
    {
      msg_error("Failed to open master key file", 
                evt_tag_str("path", szPath));
      return FALSE;
    }
  if (fread(key, 1, KEY_SIZE, file) != KEY_SIZE)
    {
      msg_error("Failed to read master key", 
                evt_tag_str("path", szPath), 
                evt_tag_int("expected_length", KEY_SIZE));
      fclose(file);
      return FALSE;
    }
  fclose(file);
  return TRUE;
}



//----------------------------------------------------------------------
// is_equal_nullvector
//
// return TRUE when buffer contains only of zeros else FALSE

gboolean is_equal_nullvector(unsigned char *buf, gsize len, gboolean debug)
{
  if (NULL == buf)
    {
      msg_error("Failed to check null vector", 
                evt_tag_str("reason", "NULL buffer"));
      return TRUE; //-- Handle NULL pointer as a nullvector
    }

  //-- Iterate through the buffer until first none zero
  for (gsize i = 0; i < len; i++)
    {
      if (buf[i] != 0)
        {
          return FALSE;
        }
    }

  //-- If the loop above completes, it means all bytes were zero
  if (TRUE == debug)
    {
      g_print("- - - - -  nullvector, buffer at %p of length %ld contains zero's only  - - - - -\n\n", (void *)buf, len);
    }

  return TRUE;
}


//----------------------------------------------------------------------
// cr_check_cpu_cfg
//
// return TRUE when correct preprossor setting else FALSE

gboolean cr_check_cpu_cfg(void)
{
  int sum_value = 0;

#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
  sum_value += 1;
  //g_print("CPU_AVX2\n");
#endif

#if defined(CPU_SSE2) && (CPU_SSE2 == 1)
  sum_value += 1;
  //g_print("CPU_SSE2\n");
#endif

#if defined( CPU_OTHER ) && (CPU_OTHER == 1)
  sum_value += 1;
  //g_print("CPU_OTHER\n");
#endif

  if ((0 == sum_value) || (1 < sum_value))
    {
      msg_error("Invalid CPU configuration", 
                evt_tag_int("configured_cpu_options", sum_value));
      return FALSE; //-- ERROR
    }
  return TRUE;
}



//----------------------------------------------------------------------
// cr_print_cpu_cfg
//
// return Show CPU cfg by terminal output

void cr_print_cpu_cfg(void)
{
#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
  // AVX2 code
  g_print("active: CPU_AVX2\n");
#elif defined(CPU_SSE2) && (CPU_SSE2 == 1)
  // SSE2 code
  g_print("active: CPU_SSE2\n");
#elif defined(CPU_OTHER) && (CPU_OTHER == 1)
  // 64-bit code
  g_print("Neither CPU_AVX2 nor CPU_SSE2 was active\n");
#else
  // #error "No valid XOR implementation defined!"
  msg_error("No valid CPU configuration defined", 
            evt_tag_str("reason", "Check CPU_AVX2, CPU_SSE2 and CPU_OTHER"));
#endif
}



//----------------------------------------------------------------------
// get_cpu_cfg_info
//
// Provides CPU preprocessor configuration as GString
//
// in is_add_new_line: Whether new line '\n' shall be added at the end of gstr
//
// The caller has to free the memory when the Gstring is not needed anymore.
// GString *gs = convert_time_diff(...
// ...
// g_string_free(gs, TRUE);
// gs = NULL;
//
// return GString with CPU config information. The caller owns the memory.

GString *get_cpu_config_info(gboolean is_add_new_line)
{
  GString *gstr = NULL;
  gstr = g_string_new("");

#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
  // AVX2 code
  g_string_append_printf(gstr, "Optimized for CPU instruction set AVX2");
#elif defined(CPU_SSE2) && (CPU_SSE2 == 1)
  // SSE2 code
  g_string_append_printf(gstr, "Optimized for CPU instruction set SSE2");
#elif defined(CPU_OTHER) && (CPU_OTHER == 1)
  // slow 64-bit code without optimization
  g_string_append_printf(gstr, "Neither AVX2 nor SSE2 CPU optimization is active");
#else
  //-- ERROR
  msg_error("No valid CPU configuration defined", 
            evt_tag_str("reason", "Check CPU_AVX2, CPU_SSE2 and CPU_OTHER"));
            
  g_string_append_printf(gstr, "\nNo valid CPU configuraton defined!\n \
                         Preprocessors CPU_AVX2, CPU_SSE2 and CPU_OTHER must be defined!\n \
                         And only one of them must be set to 1 whereas others are to be set to 0.\n\n");
#endif
  if (TRUE == is_add_new_line)
    {
      g_string_append_printf(gstr, "\n");
    }
  return gstr;
}


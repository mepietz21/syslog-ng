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


// based on THESIS_secure-logging-cr\src\shared\Crypto.c
//
//  Crypto.c
//  shared
//
//  Copyright © 2023 Airbus Commercial Aircraft
//  Created by Florian on 15.11.23.
//

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/cmac.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <glib.h>
#include <messages.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/params.h>
#endif

#include "cr_pi_shared.h"
#include "cr_crypto.h"


// The folling must be replaced due to GitHub code checker
// static unsigned char GAMMA[2 * AES_BLOCK_LEN] = {[0 ... (2 * AES_BLOCK_LEN - 1)] = PAD1};
// cr_pi_shared.h:54: #define AES_BLOCK_LEN 16

SLOGCR_STATIC_ASSERT(AES_BLOCK_LEN == 16, "Wrong_AES_Block_Size_for_provided_GAMMA_initialization");

#define FILL_16(val) val, val, val, val, val, val, val, val, \
                      val, val, val, val, val, val, val, val

static guchar GAMMA[2 * AES_BLOCK_LEN] = { FILL_16(PAD1), FILL_16(PAD1) };


static gint cr_under_PRG(guchar *seed, guint *counter, const EVP_CIPHER *cipher, guchar *buffer,
                        gint size);
static gint cr_AES_encrypt(const EVP_CIPHER *cipher, guchar *plaintext, gint plaintextSize, guchar *key,
                          guchar *iv, guchar *ciphertextBuffer);
static gint cr_exists(gint element, const gint arr[], gsize size);
static void cr_handleErrors(void);


//----------------------------------------------------------------------
// cr_CreatePRG128Context
//
// in seed: Seed to be copied into context
// returns pointer to cr_PRG128Context

cr_PRG128Context *cr_CreatePRG128Context(guchar seed[16])
{
  cr_PRG128Context *context = g_malloc0(sizeof(cr_PRG128Context));
  if (context == NULL)
    {
      // fixed
      msg_warning("Failed to allocate PRG128 context",
            evt_tag_printf("size", "%zu", sizeof(cr_PRG128Context)));
      return NULL;
    }
  context->counter = 0;
  memcpy(context->seed, seed, 16);
  return context;
}



//----------------------------------------------------------------------
// cr_CreatePRGContext
//
// in seed: Seed to be copied into context
// returns pointer to cr_PRGContext

cr_PRGContext *cr_CreatePRGContext(guchar seed[KEY_SIZE])
{
  cr_PRGContext *context = g_malloc0(sizeof(cr_PRGContext));
  if (context == NULL)
    {
     // fixed
     msg_warning("Failed to allocate PRG context",
                  evt_tag_printf("context_size", "%zu",
                                 sizeof(cr_PRGContext)));
      return NULL;
    }
  context->counter = 0;
  memcpy(context->seed, seed, KEY_SIZE);
  return context;
}



//----------------------------------------------------------------------
// cr_under_PRG
//
// PRG helper method
// in seed:
// in/out counter:
// in cipher:
// in/out  buffer:
// in size: size of buffer
//
// returns 1 when SUCCESS else 0

static gint cr_under_PRG(guchar *seed, guint *counter, const EVP_CIPHER *cipher, guchar *buffer,
                        gint size)
{
  // calculate the number of aes blocks and ceil if it is not a multiple of the AES block size.

  // new:

  if (G_UNLIKELY(NULL == seed || NULL == counter || NULL == cipher || NULL == buffer || size <= 0))
  {
    g_warning("cr_under_PRG: Invalid input parameters");
    return 0;
  }


  double temp_m = ceil((double)size / AES_BLOCK_LEN);
  guint blocks = (guint) temp_m;
  gint paddedSize = blocks * AES_BLOCK_LEN;

  guchar *input = calloc(paddedSize, sizeof(guchar));
  guchar *output = calloc(paddedSize, sizeof(guchar));

  // Each AES block contain the next higher counter sequence.
  for (gulong i = 0; i < blocks; ++i)
    {
      guchar *destinationPtr = &input[i * AES_BLOCK_LEN];
      gulong ctr = i + *counter;

      // Copy the unsigned long value to the block in the input buffer
      memcpy(destinationPtr, &ctr, sizeof(gulong));
    }

  // save to global counter.
  *counter += blocks;

  gint outputLen = cr_AES_encrypt(cipher, input, paddedSize, seed, NULL, output);
  free(input);

  // Validate output buffer length
  if (outputLen != paddedSize)
    {
      // fixed
      msg_warning("Output length does not match padded size",
                evt_tag_printf("output_len", "%zu", outputLen),
                evt_tag_printf("padded_size", "%zu", paddedSize));
      free(output);
      return 0; //-- ERROR
    }

  if (size < 0)
    {
      // fixed
      msg_warning("Negative size provided",
                evt_tag_printf("size", "%d", size));
      return 0; //--ERROR
    }

  // Fill result buffer, but only with the requested amount of random data.
  memcpy(buffer, output, (guint) size);
  free(output);

  return 1; //-- SUCCESS
}



//----------------------------------------------------------------------
// cr_PRG128
//
// Wrapper of cr_under_PRG
// in/out ctx: Pointer to instance of cr_PRG128Context
// in/out  buffer:
// in size: size of buffer
//
// returns 1 when SUCCESS else 0

int cr_PRG128(cr_PRG128Context *ctx, guchar *buffer, gint size)
{
  return cr_under_PRG(ctx->seed, &ctx->counter, EVP_aes_128_ecb(), buffer, size);
}



//----------------------------------------------------------------------
// cr_PRG
//
// Wrapper of cr_under_PRG
// in/out ctx: Pointer to instance of cr_PRGContext
// in/out  buffer:
// in size: size of buffer
//
// returns 1 when SUCCESS else 0

int cr_PRG(cr_PRGContext *ctx, guchar *buffer, gint size)
{
  return cr_under_PRG(ctx->seed, &ctx->counter, EVP_aes_256_ecb(), buffer, size);
}



//----------------------------------------------------------------------
// cr_AES_256_CTR_encrypt
//
// Wrapper of cr_AES_encrypt
// in plaintext:
// in plintextSize:
// in key:
// in iv:
// in chiphertextBuffer:
//
// returns 1 when SUCCESS else 0

int cr_AES_256_CTR_encrypt(guchar *plaintext, gint plaintextSize, guchar *key, guchar *iv,
                           guchar *ciphertextBuffer)
{
  return cr_AES_encrypt(EVP_aes_256_ctr(), plaintext, plaintextSize, key, iv, ciphertextBuffer);
}



//----------------------------------------------------------------------
// cr_AES_256_CTR_decrypt
//
// Wrapper of OpenSSL AES 256 decryption
// based on: https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption#Encrypting_the_message
// in ciphertext:
// in ciphertextSize:
// in key:
// in iv:
// in plaintextBuffer:
//
// returns Length of plain text

int cr_AES_256_CTR_decrypt(guchar *ciphertext, gint ciphertextSize, guchar *key, guchar *iv,
                           guchar *plaintextBuffer)
{
  EVP_CIPHER_CTX *ctx;

  gint len = 0;

  gint plaintextLen;
  /* Create and initialise the context */
  if (!(ctx = EVP_CIPHER_CTX_new()))
    cr_handleErrors();

  /*
   * Initialise the decryption operation. IMPORTANT - ensure you use a key
   * and IV size appropriate for your cipher
   * In this example we are using 256 bit AES (i.e. a 256 bit key). The
   * IV size for *most* modes is the same as the block size. For AES this
   * is 128 bits
   */
  if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv)) /* Failed to initialize aes in ECB mode. */
    cr_handleErrors();

  // Disable padding, the total amount of data encrypted or decrypted must then be a multiple of the block size or an error will occur.
  if (1 != EVP_CIPHER_CTX_set_padding(ctx, 0))
    cr_handleErrors();

  /*
   * Provide the message to be decrypted, and obtain the plaintext output.
   * EVP_DecryptUpdate can be called multiple times if necessary.
   */
  if (1 != EVP_DecryptUpdate(ctx, plaintextBuffer, &len, ciphertext, ciphertextSize)) /* Failed to decrypt plaintext. */
    cr_handleErrors();
  plaintextLen = len;

  /*
   * Finalise the decryption. Further plaintext bytes may be written at
   * this stage.
   */
  if (1 != EVP_DecryptFinal_ex(ctx, plaintextBuffer + len, &len))
    cr_handleErrors();
  plaintextLen += len;

  /* Clean up */
  EVP_CIPHER_CTX_free(ctx);

  return plaintextLen;
}



//----------------------------------------------------------------------
// cr_AES_encrypt
//
// Wrapper of OpenSSL AES 256 encryption
// based on: https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption#Encrypting_the_message
// in cipher:
// in plaintext:
// in plaintextSize:
// in key:
// in iv:
// in ciphertextBuffer:
//
// returns Length of encrypted text (0 in case of ERROR)

static gint cr_AES_encrypt(const EVP_CIPHER *cipher,
                           guchar *plaintext,
                           gint plaintextSize,
                           guchar *key,
                           guchar *iv,
                           guchar *ciphertextBuffer)
{
    EVP_CIPHER_CTX *ctx;
    gint len;
    gint ciphertextLen;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        msg_error("Failed to create AES cipher context");
        return 0;
    }

    if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) != 1)
    {
        msg_error("Failed to initialize AES encryption");
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    if (EVP_CIPHER_CTX_set_padding(ctx, 0) != 1)
    {
        msg_error("Failed to disable AES padding");
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    if (EVP_EncryptUpdate(ctx, ciphertextBuffer, &len,
                          plaintext, plaintextSize) != 1)
    {
        msg_error("Failed to encrypt AES data");
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    ciphertextLen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertextBuffer + len, &len) != 1)
    {
        msg_error("Failed to finalize AES encryption");
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    ciphertextLen += len;

    EVP_CIPHER_CTX_free(ctx);

    return ciphertextLen;
  }


//----------------------------------------------------------------------
// cr_exits
//
// Helper to check if a number is in an array
// in element: Number to search for
// in arr:
// in size:
// returns 1 when number found in array else 0 when not present

static gint cr_exists(gint element, const gint arr[], gsize size)
{
  for (gsize i = 0; i < size; ++i)
    {
      if (arr[i] == element)
        {
          return 1;
        }
    }
  return 0;
}


//----------------------------------------------------------------------
// cr_UniformRandomInt
//
// PRG number generation and upperbound handling to eliminate the modul bias
// in ctx: Context of cr_PRGContext
// in upperBound: upper bound for random number generation
// out: Pointer to store the generated random number
//
// returns 1 on SUCCESS and 0 on FAILURE

int cr_UniformRandomInt(cr_PRGContext *ctx, const guint upperBound, guint *out)
{
  guint64 multipleOfUpperBound;
  guint rand;
  guchar *randomBuffer;

  if (out == NULL || upperBound < 2)
    {
      // fixed
      msg_warning("Invalid arguments for cr_UniformRandomInt",
                  evt_tag_printf("upper_bound", "%u", upperBound),
                  evt_tag_printf("out_is_null", "%s", out == NULL ? "true" : "false"));
      return 0; //-- ERROR
    }

  // eliminate the modul bias
  // https://research.kudelskisecurity.com/2020/07/28/the-definitive-guide-to-modulo-bias-and-how-to-avoid-it/
  // https://github.com/jedisct1/libsodium/blob/master/src/libsodium/randombytes/randombytes.c#L145
  multipleOfUpperBound = (1ULL << 32) - ((1ULL << 32) % upperBound);
  
  randomBuffer = g_malloc0(sizeof(guint));
  if (randomBuffer == NULL)
    {
      // fixed
      msg_warning("Failed to allocate memory for randomBuffer",
                  evt_tag_printf("buffer_size", "%zu", sizeof(guint)));
      return 0; //-- ERROR
    }

  for (;;)
    {
      if (1 != cr_PRG(ctx, randomBuffer, sizeof(guint)))
        {
          // fixed
          msg_warning("PRG failed",
                      evt_tag_printf("random_buffer_size", "%zu", sizeof(guint)));
          g_free(randomBuffer);
          return 0; //-- ERROR
        }
      memcpy(&rand, randomBuffer, sizeof(guint));
      if (rand < multipleOfUpperBound)
        break;
    }

  g_free(randomBuffer);
  *out = rand % upperBound;
  return 1; //-- SUCCESS
}


//----------------------------------------------------------------------
// cr_DRN
//
// Fill array with random numbers
//
// in seed
// in the_k
// in upperBound:
// in kRandom: Array of count the_k
//
// returns 1 on SUCCESS and 0 on FAILURE

int cr_DRN(guchar seed[KEY_SIZE], const gint the_k, const gint upperBound, gint kRandom[THE_K])
{
  guint rand;
  gint i = 0;

//-- BUG FIX to avoid endless while loop. Anyhow count of log lines should much greater, e.g.: at least 4096.
  if (upperBound < the_k)
    {
      // fixed: msg_warning instead of g_warning
      msg_warning("Failed: cr_DRN, upperBound provides only %d different random numbers but the_the_k %d are needed at least to leave while loop!",
                  evt_tag_printf("upper_bound", "%d", upperBound),
                  evt_tag_printf("the_k", "%d", the_k));
      return 0; //-- ERROR
    }

  if ((2 >= upperBound) && (upperBound > INT_MAX))
    {
      // fixed: msg_warning instead of g_warning
      msg_warning("Failed: cr_DRN, upperBound out of range",
                  evt_tag_printf("upper_bound", "%d", upperBound),
                  evt_tag_printf("the_k", "%d", the_k));
      return 0; //-- ERROR
    }

  // Fill the array with -1
  // thats why the upper bound cant be larger than int_max
  for (gint r = 0; r < the_k; ++r)
    {
      kRandom[r] = -1;
    }

  cr_PRGContext *ctx = cr_CreatePRGContext(seed);
  if (NULL == ctx)
    {
      // fixed: msg_warning instead of g_warning
      msg_warning("Failed: cr_DRN, ctx is NULL!\n");
      return 0; //-- ERROR
    }

  const gint LEAVE_LOOP = the_k * 1042; //-- the_k - typical 5 - should be enough, safe
  gint a = 0;
  while (i < the_k)
    {
      //-- rand % upperbound so upperBound must be >= the_k else endless loop while
      guint rand;
      if (!cr_UniformRandomInt(ctx, upperBound, &rand))
        {
          // fixed: msg_warning instead of g_warning
          msg_warning("Failed to generate random number.");
          g_free(ctx);
          return 0;
        }
      // .. overflows and rand was not changing and caused an endless loop here!

      // check if the random number already exists in the arra of k random numbers.
      if (!cr_exists(rand, kRandom, (gsize) (guint) the_k))
        {
          kRandom[i] = rand;
          i++;
        }

      //-- ensure no endless loop when rand generation is wrong
      if (LEAVE_LOOP < ++a)
        {
          // fixed: msg_warning instead of g_warning
          msg_warning("Failed: cr_DRN, rand does not change!\n");
          g_free(ctx);
          return 0; //-- ERROR
        }
    }

  g_free(ctx);
  return 1; //-- SUCCESS
}



/*
 Two phases PRF:
 1. CMAC, which returns a 128bit MAC tag which will than be used as key for a
 2. AES-128-ECB encryption of a counter.
 */
int cr_PRF(guchar *input, gsize inputSize, guchar *key, guchar *output, guint8 outputSize)
{
  gsize outputLenCMAC;

  // add a byte for the outputSize, max len for the outputSize is 2^8 = 128
  /* guchar _input[inputSize + 1], seed[16];
  memcpy(_input, input, inputSize);

  // The output size is an input value of the PRF, and should therefore change the output of the PRF the same way, as the key or input, would do.
  // input || outputSize, set the last byte to the output size.
  _input[inputSize] = outputSize; */

  if (G_UNLIKELY(NULL == input || NULL == key || NULL == output || inputSize == 0 || outputSize == 0))
    {
      msg_warning(CR_WARNING_PREFIX, evt_tag_str("Reason", "cr_PRF: Invalid NULL or 0-length input"));
      return 0;
    }

  /*if (!cr_CMAC(key, _input, inputSize, seed, &outputLenCMAC, CMAC_LEN))
    {
      // fixed: msg_warning instead of g_warning
      msg_warning("Failed to create CMAC as seed for a PRG as output for the variable PRF.");
      return 0;
    }*/

  // fixed: 
  unsigned char *_input = g_try_malloc(inputSize + 1);
  unsigned char seed[16];
  if (NULL == _input)
    {
      msg_error(CR_ERROR_PREFIX, evt_tag_str("Reason", "Failed to allocate memory buffer"));
      return 0;

    }

  memcpy(_input, input, inputSize);
  _input[inputSize] = outputSize;

  if(!cr_CMAC(key, _input, inputSize + 1, seed, &outputLenCMAC, G_N_ELEMENTS(seed)))
    {

      msg_warning(CR_ERROR_PREFIX, evt_tag_str("Reason", "Failed to create CMAC as seed for PRG in cr_PRF"));
      g_free(_input);
      return 0;

    }

  g_free(_input);

  // stretch or cut the output of the PRF, by applying a PRG.
  cr_PRG128Context *ctx = cr_CreatePRG128Context(seed);
  if (NULL == ctx)
    {
      // fixed: msg_warning instead of g_warning
      msg_warning("Failed: ctx is NULL.");
      return 0;
    }
  if (!cr_PRG128(ctx, output, outputSize))
    {
      // fixed: msg_warning instead of g_warning
      msg_warning("Failed to create PRF output, when using PRG.");
      g_free(ctx);
      return 0;
    }

  g_free(ctx);
  return 1;
}



int cr_KeyEvolution(guchar *key, guchar *nextKey)
{
  return cr_PRF(GAMMA, 32, key, nextKey, KEY_SIZE);
}



int cr_DeriveSubKeys(guchar masterSessionkey[KEY_SIZE], guchar encKey[KEY_SIZE],
                     guchar drnKey[KEY_SIZE], guchar tagKey[KEY_SIZE], guchar idKey[KEY_SIZE])
{
  cr_PRGContext *ctx = cr_CreatePRGContext(masterSessionkey);
  guchar *output = g_malloc0(4 * KEY_SIZE);

  // fixed: Warning for memory allocation
  if (ctx == NULL || output == NULL)
    {
      msg_warning("Failed to allocate memory for subkey derivation",
                  evt_tag_printf("output_size", "%zu", 4 * sizeof(guchar)));
      g_free(ctx);
      g_free(output);
      return 0;
    }

  // fixed: msg_warning instead of g_warning
  if (cr_PRG(ctx, output, 4 * KEY_SIZE) != 1)
    {
      msg_warning("Failed to derive subkeys");
      g_free(ctx);
      g_free(output);
      return 0;
    }

  memcpy(encKey, output, KEY_SIZE);
  memcpy(drnKey, output + KEY_SIZE, KEY_SIZE);
  memcpy(tagKey, output + (2 * KEY_SIZE), KEY_SIZE);
  memcpy(idKey, output + (3 * KEY_SIZE), KEY_SIZE);

  g_free(ctx);
  g_free(output);

  return 1;
}


int cr_GenerateMasterKey(guchar *masterKey)
{
  return RAND_bytes(masterKey, KEY_SIZE);
}


int cr_GenerateIV(guchar *iv)
{
  return RAND_bytes(iv, IV_SIZE);
}


int cr_CMAC(guchar *key, guchar *input, gsize inputSize, guchar *output, gsize *outputSize,
            gsize maxOutputSize/* Prevent buffer overflows, in the case that the maximal possible outbut buffer size is smaler than the actual output buffer. */)
{
  EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
  if (mac == NULL)
    {
      // fixed: msg_warning
      msg_warning("Failed to fetch CMAC");
      return 0; //-- ERROR
    }

  EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);

  if (!ctx)
    {
      // fixed: msg_warning
      msg_warning("Failed to create MAC ctx.");
      EVP_MAC_free(mac);
      return 0; //-- ERROR
    }

  // Sets the name of the underlying cipher to be used. The mode of the cipher must be CBC.
  // https://www.openssl.org/docs/man3.1/man7/EVP_MAC-CMAC.html
  OSSL_PARAM params[2];
  params[0] = OSSL_PARAM_construct_utf8_string("cipher", "aes-256-cbc", 0);
  params[1] = OSSL_PARAM_construct_end();

  // braucht einen Array, nicht nur ein pointer auf einen Parameter.
  if (EVP_MAC_CTX_set_params(ctx, params) != 1)
    {
      // fixed: msg_warning
      msg_error("Failed to set CMAC parameters");
      // free
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return 0; //-- ERROR
    }

  if (EVP_MAC_init(ctx, key, KEY_SIZE, NULL) != 1)
    {
      msg_error("Failed to init CMAC.");
      // free
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return 0; //-- ERROR
    }

  if (EVP_MAC_update(ctx, input, inputSize) != 1)
    {
      msg_error("Failed to update CMAC.");
      // free
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return 0; //-- ERROR
    }

  // If the maxOutputSize is to small, to hold the output -> the mission will be aborted.
  if (EVP_MAC_final(ctx, output, outputSize, maxOutputSize) != 1)
    {
      msg_error("Failed to create CMAC.", 
        evt_tag_printf("max_output_size", "%zu", maxOutputSize));
      // free
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return 0; //-- ERROR
    }

  // free
  EVP_MAC_CTX_free(ctx);
  EVP_MAC_free(mac);
  return 1; //-- SUCCESS
}


void cr_handleErrors(void)
{
  msg_error("Cryptographic operation failed");
  abort();
}


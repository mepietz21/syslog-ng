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


#ifndef CR_CRYPTO
#define CR_CRYPTO

#include <stdio.h>
#include <glib.h>
#include <openssl/evp.h>
#include "cr_pi_shared.h"

// moved to cr_pi_shared.h because used for cr_logger and cr_verifier
// #define CMAC_LEN 16 //-- this was taken form the c file

// #define AES_BLOCK_LEN 16
// #define KEY_SIZE 32
// #define IV_SIZE 16
// #define THE_K 5      //-- original only K
// #define THE_C 1.1244 //-- original only C, from PIShared.h

// crypto constants with a large hamming distance
// 4 constants of 1 byte, with a Hamming Distance of 4
// #define PAD1 0xd
// #define PAD2 0x3b
// #define PAD3 0x51
// #define PAD4 0xcb

//static unsigned char GAMMA[2 * AES_BLOCK_LEN] = {[0 ... (2 * AES_BLOCK_LEN -1)] = PAD1};
//static unsigned char GAMMA_DASH[AES_BLOCK_LEN] = {[0 ... (AES_BLOCK_LEN -1)] = PAD2};

// PRF stuff
typedef struct cr_PRG128Context
{
  guint counter; // internal counter, which will be increased for each AES block added within the current context.
  guchar seed[16]; // secure seed.
} cr_PRG128Context;

typedef struct cr_PRGContext
{
  guint counter; // internal counter, which will be increased for each AES block added within the current context.
  guchar seed[KEY_SIZE]; // secure seed.
} cr_PRGContext;

cr_PRGContext *cr_CreatePRGContext(guchar seed[KEY_SIZE]);
cr_PRG128Context *cr_CreatePRG128Context(guchar seed[16]);

/*
 * Function: PRG
 * -------------
 * A AES_256_CTR based Pseudo Random Generator.
 *
 * ctx:  PRG Context.
 * buffer: which will hold the random data.
 * size: the amount of random data, and the size of the buffer.
 *
 * returns: 0 on failure and 1 on success.
 */
int cr_PRG(cr_PRGContext *ctx, guchar *buffer, gint size);

/*
 * Function: PRG
 * -------------
 * A AES_128_CTR based Pseudo Random Generator.
 *
 * ctx:  PRG Context.
 * buffer: which will hold the random data.
 * size: the amount of random data, and the size of the buffer.
 *
 * returns: 0 on failure and 1 on success.
 */
int cr_PRG128(cr_PRG128Context *ctx, guchar *buffer, gint size);

// key stuff:

/*
 * Function: KeyEvolution
 * ----------------------
 * K_{i+1} = PRF_{K_i}(\gamma)
 */
int cr_KeyEvolution(guchar *key, guchar *nextKey);

/*
 * Function: DeriveSubKeys
 * -----------------------
 * Derives the different sub keys from the current seassion key.
 */
int cr_DeriveSubKeys(guchar masterSessionkey[KEY_SIZE], guchar encKey[KEY_SIZE],
                     guchar drnKey[KEY_SIZE], guchar tagKey[KEY_SIZE], guchar idKey[KEY_SIZE]);
/*
 * Funtion: GenerateMasterKey
 * --------------------------
 * Generates a new truely random bit string.
 */
int cr_GenerateMasterKey(guchar *masterKey);

/*
 * Funtion: GenerateIV
 * --------------------------
 * Generates a new truely random bit string.
 */
int cr_GenerateIV(guchar *iv);

/*
 * Function: PRF
 * -------------
 * Pseudo Random Function, based on CMAC (AES_256) and stretches the output via PRG_128.
 *
 * input: input of the PRF.
 * inputSize: size of the input.
 * key: 256 bit large key of the PRF.
 * output: buffer holdung the PRF result.
 * outputSize: the requested amount of output bytes (max 256).
 *
 * returns: 0 on failure and 1 on success.
 */
int cr_PRF(guchar *input, gsize inputSize, guchar *key, guchar *output, guint8 outputSize);

// Random things:

/*
 * Function: UniformRandomInt
 * --------------------------
 * Generates a uniformly distributed random number, within a upper bound. Inclusive 0 exclusive the upper bound.
 * [0, upperBound)
 * The random number is based on AES based PRG.
 *
 * ctx: Context of the underlying PRG.
 * upperBound: upper bound.
 *
 * returns: the uniformly distributed random number.
 */
int cr_UniformRandomInt(cr_PRGContext *ctx, const guint upperBound, guint *out);



/*
 * Funtion: DRN
 * ------------
 * Deterministic Distinct Random Number Generator.
 * Generates a list of k distinct random numbers. [0, upperBound) Inclusive 0 exclusive the upper bound.
 * seed: a secure 256 bit large seed
 * k: the number of distinct random numbers withing the given bound.
 * upperBound: the upper bound.
 * kRandom: array holding the k distinct random numbers, of size k.
 *
 * returns: 0 on failure and 1 on success.
 */
//int DRN_thesis(unsigned char *seed, int k, const int upperBound, int *kRandom);
int cr_DRN(guchar seed[KEY_SIZE], const gint the_k, const gint upperBound, gint kRandom[THE_K]);



// AES stuff:

/*
 * Function: AES_256_CTR_encrypt
 * AES_256_CTR encryption, without padding (!), the plaintext has to be a multiple of the AES block legth (16 byte).
 *
 * plaintext: plaintext.
 * plaintextSize: plaintext size, has to be a multiple of the AES block length.
 * key: encryption key.
 * iv: Initialization Vector 16 byte!.
 * ciphertextBuffer: ciphertext buffer, with the same size of the plaintext.
 *
 * returns: the ciphertext length.
 */
int cr_AES_256_CTR_encrypt(guchar *plaintext, gint plaintextSize, guchar *key, guchar *iv,
                           guchar *ciphertextBuffer);

/*
 * Function: AES_256_CTR_decrypt
 * AES_256_CTR decryption, without padding (!), the ciphertext has to be a multiple of the AES block legth (16 byte).
 *
 * ciphertextBuffer: ciphertext buffer, with the same size of the plaintext.
 * ciphertextSize: ciphertext size, has to be a multiple of the AES block length.
 * key: decryption key.
 * iv: Initialization Vector 16 byte!.
 * plaintext: plaintext.
 *
 * returns: the plaintext length.
 */
int cr_AES_256_CTR_decrypt(guchar *ciphertext, gint ciphertextSize, guchar *key, guchar *iv,
                           guchar *plaintextBuffer);

/*
 * Funtion: CMAC
 * -------------
 * AES_256_CBC based MAC.
 *
 * key: MAC key.
 * input: message.
 * inputSize: message size.
 * output: the MAC.
 * outputSize: will return 16.
 * maxOutputSize: the size of the output buffer should be larger than 16 bytes.
 *
 * returns: 0 on failure and 1 on success.
 */
int cr_CMAC(guchar *key, guchar *input, gsize inputSize, guchar *output, gsize *outputSize,
            gsize maxOutputSize);
#endif /* cr_crypto_h */

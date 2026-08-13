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


#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cr_pi_types.h"
#include "cr_pi_shared.h"
#include "cr_matrix.h"
#include "cr_plain_gauss_helper.h"



//-- pgh: used as namespace PlainGaussHelper replacement


//-- quark for g_set_error
GQuark gauss_error_quark (void);
G_DEFINE_QUARK (GaussErrorQuark, gauss_error)
#define GAUSS_ERROR gauss_error_quark ()
typedef enum
{
  GAUSS_ERROR_GENERAL = 1001,
  GAUSS_ERROR_ALLOC = 1002
} GaussErrorCode;



//----------------------------------------------------------------------
// add_to_protocol_timediff
//
// helper to write time consumption into protocol file
// NULL pointers are ignored silently and nothing is written into protocol file
//
// in ctx: Context with initialzed open file pointer or NULL
// in ndiff: Time difference in milliseconds
// in szText: String that contains information what took the time or NULL
//
// return -

void add_to_protocol_timediff(cr_VerifierContext *ctx, uint64_t ndiff, const char *szText)
{
  if (NULL != ctx)
    {
      if (NULL != ctx->protocolFile)
        {
          GString *gstr = convert_time_diff(ndiff, szText);
          if (NULL != gstr)
            {
              fprintf(ctx->protocolFile, "%s", gstr->str);
              g_string_free(gstr, TRUE);
            }
        }
    }
}



//----------------------------------------------------------------------
// xor_buffers
//
// XOR depending on CPU configuration
// Precondition: Buffers do provide aligned memory
//
// Preprocessors are set by rebuildall.sh and configure script by
// option --with-arch
//
// using SSE2 (available on most CPU's):
//../configure --prefix=$HOME/Software/install --with-arch=sse2
//
// using AVX2 (fastes, 32-byte-aligned memory needed, only modern CPU's)
//../configure --prefix=$HOME/Software/install --with-arch=avx2
//
// when executing CPU does neither provide AVX2 nor SSE2, configure
// is to be called without --with-arch option:
//../configure --prefix=$HOME/Software/install
//
// in buf_a: 32-bit-aligned buffer that is manipulated by xor
// in buf_b: 32-bit-aligned buffer
//
// return -

#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
__attribute__((target("avx2")))
#endif
void xor_buffers(void *buf_a, const void *buf_b, gsize len)
{
  if (!buf_a || !buf_b)
    {
      msg_error("invalid pointer(s): a: %p, b: %p", buf_a, buf_b);
      return; // g_error likely terminates, but good practice to return
    }

  //-- Use pointers to 8-bit integers for byte-level access
  uint8_t *p_a = (uint8_t *)buf_a;
  const uint8_t *p_b = (const uint8_t *)buf_b;
  gsize i = 0;

#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
  //-- Process 32-byte chunks with AVX2
  for (i = 0; i + 31 < len; i += 32)
    {
      //-- NOTE
      //   Instead of using _mm256_loadu_si256 and _mm256_storeu_si256,
      //   here _mm256_load_si256 and _mm256_store_si256 are used, because
      //   it has been ensured that only 32-bit-aligned memory is provided
      //   to this function.
      //   This makes AVX2 a bit faster than SSE2.
      //   WHEN THIS FUNCTION IS CALLED FOR UNALIGNED MEMORY, THE
      //   PROGRAM WILL COREDUMP.
      __m256i chunk_a = _mm256_load_si256((__m256i *)(p_a + i));
      __m256i chunk_b = _mm256_load_si256((__m256i *)(p_b + i));
      chunk_a = _mm256_xor_si256(chunk_a, chunk_b);
      _mm256_store_si256((__m256i *)(p_a + i), chunk_a);
    }

#elif defined(CPU_SSE2) && (CPU_SSE2 == 1)
  //-- Process 16-byte chunks with SSE2
  for (i = 0; i + 15 < len; i += 16)
    {
      __m128i chunk_a = _mm_loadu_si128((__m128i *)(p_a + i));
      __m128i chunk_b = _mm_loadu_si128((__m128i *)(p_b + i));
      chunk_a = _mm_xor_si128(chunk_a, chunk_b);
      _mm_storeu_si128((__m128i *)(p_a + i), chunk_a);
    }
#endif

  //-- Cleanup loop for any remaining bytes and process all bytes in case CPU_OTHER
  //   when code neither can use AVX2 nor SSE2
  for (; i < len; ++i)
    {
      p_a[i] ^= p_b[i];
    }
}



//----------------------------------------------------------------------
// cr_pgh_Solve
//
// Provide pivot function of given matrix of type cr_BMatrixType
// The Caller is responsible for freeing returned array (see function
// free_GPtrArray_cr_XOR_TYPE(NULL, &gpa_c);
//
// in Mat: Pointer to struct cr_BMatrixType
// in gpa: Array with list of pointers to pointers to cr_XOR_TYPE buffers
// in debug: Flag whether verbose logging
// in ctx: Context providing open protocol file for logging into file
//
// return Created Array if successful else NULL

//-- std::vector<PI::XOR_TYPE> Solve(BMatrixType *M, std::vector<PI::XOR_TYPE> &v, bool debug = false);
GPtrArray *cr_pgh_Solve(struct cr_BMatrixType *Mat, GPtrArray *gpa, gboolean debug, cr_VerifierContext *ctx)
{
  if ((NULL == Mat) || (NULL == gpa))
    {
      msg_warning("cr_pgh_Solve, ERROR, Nullpointer, Mat: %p, gpa: %p",
                  (void *)Mat, (void *)gpa);
    }

  // fixed from g_printf
  msg_info("cr_pgh_Solve, Mat->rows: %d, Mat->colsInBits: %d, Mat->buckets: %d, gpa->len: %d",
           Mat->rows, Mat->colsInBits, Mat->buckets, gpa->len);

  uint64_t ndiff; //-- time diff in milliseconds
  //-- ForwardReduction ---
  struct timespec ts1 =  get_ts_now();
  //-- BMatrixType *I = BMatrixType::I(M->rows);
  struct cr_BMatrixType *Imat = cr_BMatrix_I(Mat->rows);
  cr_pgh_print(Mat, debug);
  //
  //
  cr_pgh_ForwardReduction(Mat, Imat);
  //
  //
  struct timespec ts2 =  get_ts_now();
  const char  *szGaussForward = "\n-- Gaussian Elimination (Forward Reduction)";
  ndiff = get_time_diff_in_milliseconds(ts1, ts2, szGaussForward);
  add_to_protocol_timediff(ctx, ndiff, szGaussForward);

  //-- print(*M, debug);
  cr_pgh_print(Mat, debug);
  int Rank = cr_pgh_RankOf(Mat);
  g_print("Detected Rank: %d\n", Rank);
  //-- ApplyBookkeeping ---
  ts1 =  get_ts_now();
  //-- std::vector<PI::XOR_TYPE> _v = ApplyBookkeeping(*I, v);
  GError *error = NULL;
  //
  //
  GPtrArray *gpaAB = cr_pgh_ApplyBookkeeping(Imat, gpa, &error);
  //
  //
  if (NULL == gpaAB)
    {
      msg_error("Failed gpaAB, cr_pgh_ApplyBookkeeping: %s",
                    error->message);
      g_error_free(error); //-- ERROR
    }
  else
    {
      ts2 =  get_ts_now();
      const char  *szGaussBook = "\n-- Gaussian Elimination (Bookkeeping)";
      ndiff = get_time_diff_in_milliseconds(ts1, ts2, szGaussBook);
      add_to_protocol_timediff(ctx, ndiff, szGaussBook);
    }

  //-- BackSubstitution ---
  ts1 =  get_ts_now();
  error = NULL;
  //-- std::vector<PI::XOR_TYPE> c = BackSubstitution(*M, _v);
  //
  //
  GPtrArray *gpa_c = cr_pgh_BackSubstitution(Mat, gpaAB, &error);
  //
  //
  if (NULL == gpa_c)
    {
      msg_error("Failed gpaAB, cr_pgh_ApplyBookkeeping: %s",
                    error->message);
      g_error_free(error); //-- ERROR
    }
  else
    {
      ts2 =  get_ts_now();
      const char  *szGaussBackSub = "\n-- Gaussian Elimination (Back Substitution)";
      ndiff = get_time_diff_in_milliseconds(ts1, ts2, szGaussBackSub);
      add_to_protocol_timediff(ctx, ndiff, szGaussBackSub);
    }

  //-- cleanup
  cr_BMatrix_destructor_dyn(&Imat);
  free_GPtrArray_cr_XOR_TYPE(&gpaAB);

  return gpa_c; //-- caller is responsible for freeing array
}



//----------------------------------------------------------------------
// cr_pgh_Pivot
//
// Provide pivot function of given matrix of type cr_BMatrixType
// in Mat: Pointer to struct cr_BMatrixType
// in currentRow: start row for loop
// in currentCol: col to check whether bit is set
// return -1 if none found, row index for first found

//-- int Pivot(BMatrixType *M, int &currentRow, int &currentCol);
int cr_pgh_Pivot(struct cr_BMatrixType *Mat, int currentRow, int currentCol)
{
  //g_print("cr_pgh_Solve, Enter, currentRow: %d, currentCol: %d\n", currentRow, currentCol);
  for (int i = currentRow; i < Mat->rows; ++i)
    {
      //-- if is bit
      if (TRUE == cr_BMatrix_operator_bracket(Mat, i, currentCol))
        {
          return i;
        }
    }
  return -1;
}



//----------------------------------------------------------------------
// cr_pgh_ForwardReduction
//
// in/out Mat: Pointer to struct cr_BMatrixType
// in/out Imat: Pointer to struct cr_BMatrixType
// return -

void cr_pgh_ForwardReduction(struct cr_BMatrixType *Mat, struct cr_BMatrixType *Imat)
{
  g_print("cr_pgh_ForwardReduction Mat->rows: %d, Mat->colsInBits: %d, Mat->buckets: %d\n", Mat->rows, Mat->colsInBits,
          Mat->buckets);
  g_print("cr_pgh_ForwardReduction Imat->rows: %d, Imat->colsInBits: %d, Imat->buckets: %d\n", Imat->rows,
          Imat->colsInBits, Imat->buckets);

  int null_col_counter = 0;
  int current_row = 0;

  while ((current_row + null_col_counter) < Mat->colsInBits)
    {
      if ( (0 == ( (current_row + null_col_counter) & 511)) || ( (current_row + null_col_counter + 1) == Mat->colsInBits))
        {
          g_print("\x1b[2K\r  ForwardReduction %d of %d", (current_row + null_col_counter + 1), Mat->colsInBits);
        }

      int current_col = current_row + null_col_counter;
      int new_pivot_index = cr_pgh_Pivot(Mat, current_row, current_col);

      if (-1 == new_pivot_index)
        {
          null_col_counter++;
          continue; // stop current iteration and check the next col for 1s
        }
      if (new_pivot_index != current_row)
        {
          //-- Mat->swapRows(current_row, new_pivot_index);
          cr_BMatrix_swapRows(Mat, current_row, new_pivot_index);
          //-- Imat->swapRows(current_row, new_pivot_index);
          cr_BMatrix_swapRows(Imat, current_row, new_pivot_index);
        }

      for (int row = current_row + 1; row < Mat->rows; ++row)
        {
          // if 1
          //if (!(*M)(row,current_col)) {
          g_assert(current_col >= 0 && current_col < Mat->colsInBits);
          if ( FALSE == cr_BMatrix_operator_bracket(Mat, row, current_col))
            {
              continue;
            }

          // XOR
          int current_bucket = current_row / B_B_BITS;
          for (int c = current_bucket; c < Mat->buckets; ++c)
            {
              int index = row * Mat->buckets + c;
              //-- Mat->data[index] = Mat->data[index] ^ Mat->data[current_row * Mat->buckets + c];
              Mat->data[index] = cr_B256_operatorXOR(&(Mat->data[index]), &(Mat->data[current_row * Mat->buckets + c]));
            }

          for (int c = 0; c < Imat->buckets; ++c)
            {
              int index = row * Imat->buckets + c;
              //-- Imat->data[index] = Imat->data[index] ^ Imat->data[current_row * Imat->buckets + c];
              Imat->data[index] = cr_B256_operatorXOR(&(Imat->data[index]), &(Imat->data[current_row * Imat->buckets + c]));
            }
        }
      current_row++;
    }
  g_print("\n");
}



//----------------------------------------------------------------------
// create_GPtrArray_cr_XOR_TYPE
//
// in count:
// in error:
//
// Helper function: Kind of ctor std::vector<cr_XOR_TYPE> C replacement
// Creates pointer array and fills it with count empty buffer of type
// cr_XOR_TYPE and ensure that memory is alinged to 32 bytes.
// The caller owns the memory. He is responsible for freeing it.
// in count: Count of buffers of type cr_XOR_TYPE and linked in array
// in/out: error, Pointer-Pointer to GError (only set when alloc fails)
//
// returns Pointer to GPtrArray

GPtrArray *create_GPtrArray_cr_XOR_TYPE(gsize count, GError **error)
{
  GPtrArray *gpa = g_ptr_array_new();
  if (NULL == gpa)
    {
      msg_error("failed: create_GPtrArray_cr_XOR_TYPE, g_ptr_array_new");
      return NULL; //-- ERROR
    }
  g_ptr_array_set_size(gpa, count);
  //-- Allocate ONE single, contiguous, and aligned block for ALL elements
  gsize total_size = count * sizeof(cr_XOR_TYPE);
  cr_XOR_TYPE *data_block = (cr_XOR_TYPE *)aligned_alloc(AVX2_ALIGNMENT, total_size);
  if (NULL == data_block)
    {
      if ((NULL != error) && (NULL == *error))
        {
          g_set_error(error, GAUSS_ERROR, GAUSS_ERROR_ALLOC, "Failed to allocate aligned block of %zu bytes", total_size);
        }
      g_ptr_array_free(gpa, FALSE); // Free the container, not the (unallocated) data
      return NULL; //-- ERROR
    }
  memset(data_block, 0, total_size);
  //-- Populate the GPtrArray with pointers into the single block
  for (guint i = 0; i < count; i++)
    {
      gpa->pdata[i] = &data_block[i];
    }
  return gpa;
}



//----------------------------------------------------------------------
// free_GPtrArray_cr_XOR_TYPE
//
// Destructor of GPtrArray (for arrays created by create_GPtrAray_cr_XOR_TYPE)
//
// in/put gpa: Pointer-Pointer to GPtrArray which is freeed inclusive content
//             of type cr_XOR_TYPE
//
// return -

void free_GPtrArray_cr_XOR_TYPE(GPtrArray **gpa)
{
  if (gpa == NULL || *gpa == NULL)
    {
      return;
    }
  if ((*gpa)->len > 0)
    {
      void *data_block = (*gpa)->pdata[0];
      if (data_block != NULL)
        {
          g_free(data_block);
        }
    }
  g_ptr_array_free(*gpa, TRUE);
  *gpa = NULL;
}


//----------------------------------------------------------------------
// cr_pgh_ApplyBookkeeping
//
// Provide array of buffers based on Identiy Matrix and another array of
// buffers.
// The caller owns returned array. He is responsible for calling free
// and g_ptr_array_free
// in Imat: Pointer to struct cr_BMatrixType
// in gpa: Pointer to array of pointers to buffers to type cr_XOR_TYPE
// return Pointer to array of pointers to buffers of type cr_XOR_TYPE

//-- std::vector<PI::XOR_TYPE> ApplyBookkeeping(BMatrixType &I, std::vector<PI::XOR_TYPE> &v);
GPtrArray *cr_pgh_ApplyBookkeeping(struct cr_BMatrixType *Imat, GPtrArray *gpa, GError **error)
{
  gboolean is_verbose = FALSE;
  if ((NULL == Imat) || (NULL == gpa))
    {
      msg_error("Failed: cr_pgh_ApplyBookkeeping, NULL pointer in argument list: Imat: %p, gpa: %p", + (void *)Imat, (void *)gpa);
      return NULL; //-- ERROR
    }
  g_print("cr_pgh_ApplyBookkeeping Imat->rows: %d, Imat->colsInBits: %d, Imat->buckets: %d\n", Imat->rows,
          Imat->colsInBits, Imat->buckets);
  g_print("cr_pgh_ApplyBookkeeping, gpa->len: %d\n", gpa->len);
  if ( gpa->len < (guint) Imat->colsInBits )
    {
      msg_error("Failed: cr_pgh_ApplyBookkeeping gpa->len: %d < Imat->colsInBits: %d", + gpa->len, Imat->colsInBits);
      return NULL; //-- ERROR
    }

  //-- std::vector<PI::XOR_TYPE> _v(I.rows, PI::XOR_TYPE{});
  GError *err = NULL;
  GPtrArray *gpa_out = create_GPtrArray_cr_XOR_TYPE(Imat->rows, &err);
  if (NULL != err)
    {
      msg_error("Failed: cr_pgh_ApplyBookkeeping %s", err->message);
      g_error_free(err);
      return NULL; //-- ERROR
    }

  if (gpa_out->len < (guint) Imat->rows)
    {
      msg_error("Failed: cr_pgh_ApplyBookkeeping gpa_out->len: %d < Imat->rows: %d", + gpa->len, Imat->rows);
      return NULL;
    }

  //-- Apple Metal-Shading-Language-Specification.pdf: 8 bytes unsigned long == uint64_t
  //-- const gsize ulongSize = sizeof(unsigned long);
  //-- const gsize ulongLen = CIPHERTEXT_LEN / ulongSize;
  const gsize CHUNK_SIZE = sizeof(uint64_t); //-- 8
  const gsize COUNT_OF_CHUNKS = CIPHERTEXT_LEN / CHUNK_SIZE; //-- 1056 / 8 = 132

  if (TRUE == is_verbose)
    g_print("cr_pgh_ApplyBookkeeping, CHUNK_SIZE: %d, COUNT_OF_CHUNKS: %d, rows: %d, colsInBits: %d\n", CHUNK_SIZE,
            COUNT_OF_CHUNKS, Imat->rows, Imat->colsInBits);
  unsigned char *ptr;
  unsigned char *ptr_out;
  for (int row = 0; row < Imat->rows; ++row)
    {
      if ( (0 == (row & 511)) || (row + 1 == Imat->rows))
        {
          g_print("\x1b[2K\r  Bookkeeping %d of %d", row + 1, Imat->rows);
        }
      for (int col = 0; col < Imat->colsInBits; ++col)
        {
          //-- if (I(row, col)) {
          if (TRUE == cr_BMatrix_operator_bracket(Imat, row, col)) //-- if is bit
            {
              // auto* vRowAsUlong = reinterpret_cast<unsigned long*>(v[col].data());
              // auto* _vRowAsUlong = reinterpret_cast<unsigned long*>(_v[row].data());
              // // XOR
              // for (int i = 0; i < ulongLen; ++i) {
              //    _vRowAsUlong[i] ^= vRowAsUlong[i];
              //}
              ptr = (unsigned char *) g_ptr_array_index(gpa, col);
              ptr_out = (unsigned char *) g_ptr_array_index(gpa_out, row);
              xor_buffers(ptr_out, ptr, CIPHERTEXT_LEN);
            }
        }
    }
  g_print("\n");
  if (TRUE == is_verbose)
    g_print("cr_pgh_ApplyBookkeeping, Leave\n");
  return gpa_out;
}



//----------------------------------------------------------------------
// cr_pgh_BackSubstitution
//
// Provide array of buffers based on Matrix and another array of
// buffers.
// The caller owns returned array. He is responsible for calling free
// and g_ptr_array_free
// in Mat: Pointer to struct cr_BMatrixType
// in gpa: Array of pointers to buffers
// return Pointer to newly allocated array of pointers to newly allocated buffers of type cr_XOR_TYPE

//-- std::vector<PI::XOR_TYPE> BackSubstitution(BMatrixType &M, std::vector<PI::XOR_TYPE> &v);
GPtrArray *cr_pgh_BackSubstitution(struct cr_BMatrixType *Mat, GPtrArray *gpa, GError **error)
{
  gboolean is_verbose = FALSE;
  if ((NULL == Mat) || (NULL == gpa))
    {
      msg_error("failed: cr_pgh_BackSubstitution, NULL pointer in argument list: Mat: %p, gpa: %p", + (void *)Mat, (void *)gpa);
      return NULL; //-- ERROR
    }
  g_print("cr_pgh_BackSubstituion, Mat->rows: %d, Mat->colsInBits %d, Mat->buckets: %d\n", Mat->rows, Mat->colsInBits,
          Mat->buckets);
  g_print("cr_pgh_BackSubstituion, gpa->len: %d\n", gpa->len);
  if ( gpa->len < (guint) Mat->colsInBits )
    {
      msg_error("failed: cr_pgh_BackSubstituioin gpa->len: %d < Mat->colsInBits: %d", + gpa->len, Mat->colsInBits);
      return NULL; //-- ERROR
    }

  //-- std::vector<PI::XOR_TYPE> ci (M.colsInBits, PI::XOR_TYPE{});
  GError *err = NULL;
  GPtrArray *gpa_out = create_GPtrArray_cr_XOR_TYPE(Mat->rows, &err); //-- ci
  if (NULL != err)
    {
      msg_error("failed: cr_pgh_BackSubstitution %s", err->message);
      g_error_free(err);
      return NULL; //-- ERROR
    }

  if (gpa_out->len < (guint) Mat->rows )
    {
      msg_error("failed: cr_pgh_BackSubstituion gpa_out->len: %d < Mat->rows: %d", + gpa->len, Mat->rows);
      return NULL; //-- ERROR
    }

  //-- Apple Metal-Shading-Language-Specification.pdf: 8 bytes unsigned long == uint64_t
  //-- const gsize ulongSize = sizeof(unsigned long);
  //-- const gsize ulongLen = CIPHERTEXT_LEN / ulongSize;
  const gsize CHUNK_SIZE = sizeof(uint64_t); //-- 8
  const gsize COUNT_OF_CHUNKS = CIPHERTEXT_LEN / CHUNK_SIZE; //-- 1056 / 8 = 132
  if (TRUE == is_verbose)
    g_print("cr_pgh_BackSubstituion, CHUNK_SIZE: %d, COUNT_OF_CHUNKS: %d, rows: %d, colsInBits: %d\n", CHUNK_SIZE,
            COUNT_OF_CHUNKS, Mat->rows, Mat->colsInBits);
  unsigned char *ptr;
  unsigned char *ptr_out;
  // we skip the rows which contains no cs
  for (int r = 0; r < Mat->colsInBits; ++r)
    {
      if ( (0 == (r & 511)) || (r + 1 == Mat->colsInBits) )
        {
          g_print("\x1b[2K\r  BackSubstitution %d of %d", r + 1, Mat->colsInBits);
        }

      int row = Mat->colsInBits - 1 - r; // use main diagonal
      // start at the main diagonal
      // -1 one because we are starting at index 0 and colsInBits is the a number start counting at 1
      // It is not needed to check further than the main diagonal
      // the matrix is overdetermined that why we sometimes check over the main diagonal

      // The first element should be the 1 in the main diagonal, this is the single new entry which is unknown,
      // and has to be taken from the vector v.
      // in some cases it is possible that there is a 0 at the main diagonal, thats why we are looping till
      // we found the first 1, starting at the main diagonal.

      // begin at the diagonal.
      int col;
      for (col = row; col < Mat->colsInBits; ++col)
        {
          //-- if (M(row, col)) {
          if (TRUE == cr_BMatrix_operator_bracket(Mat, row, col)) //-- if is bit
            {
              // Reinterpret as unsigned long arrays for XOR
              //auto* vColAsUlong = reinterpret_cast<unsigned long*>(v[col].data());
              //auto* ciRowAsUlong = reinterpret_cast<unsigned long*>(ci[row].data());
              //for (int i = 0; i < ulongLen; ++i) {
              //    ciRowAsUlong[i] ^= vColAsUlong[i]; // The first vector will be taken from the original v
              //}
              ptr = (unsigned char *) g_ptr_array_index(gpa, col); //-- v
              ptr_out = (unsigned char *) g_ptr_array_index(gpa_out, row); //-- ci
              xor_buffers(ptr_out, ptr, CIPHERTEXT_LEN);
              col++;
              break; // we found the first Vektor now get the rest out of th c_results
            }
        }
      // XORing it with the current state of c_i, will remove all the vectors, that have been combined with it.
      for (; col < Mat->colsInBits; ++col)
        {
          // Check if there is a 1 at this index, and if so XOR the vector at this index on c_i
          //-- if (M(row, col)) {
          if (TRUE == cr_BMatrix_operator_bracket(Mat, row, col)) //-- if is bit
            {
              // Reinterpret as unsigned long arrays for XOR
              // auto *ciColAsUlong = reinterpret_cast<unsigned long *>(ci[col].data());
              //auto *ciRowAsUlong = reinterpret_cast<unsigned long *>(ci[row].data());
              //for (int i = 0; i < ulongLen; ++i)
              //  {
              //    ciRowAsUlong[i] ^= ciColAsUlong[i]; // XOR with already know vectors, to unpeel the current.
              //  }
              ptr = (unsigned char *) g_ptr_array_index(gpa_out, col); //-- ci
              ptr_out = (unsigned char *) g_ptr_array_index(gpa_out, row); //-- ci
              xor_buffers(ptr_out, ptr, CIPHERTEXT_LEN);
            } //-- if is bit
        } //-- for loop 2
    }
  g_print("\n");
  return gpa_out; //-- ci
}



//----------------------------------------------------------------------
// cr_pgh_RankOf
//
// Discovers the rank of given matrix
// in Mat: Pointer to struct cr_BMatrixType
// return integer value rank

//-- int RankOf(BMatrixType &m);
int cr_pgh_RankOf(struct cr_BMatrixType *Mat)
{
  g_print("cr_pgh_RangOf, Mat->rows: %d, Mat->colsInBits: %d, Mat->buckets: %d\n", Mat->rows, Mat->colsInBits,
          Mat->buckets);
  for (int r = Mat->rows - 1; r >= 0; --r)
    {
      for (int c = Mat->colsInBits - 1; c >= r; --c)
        {
          if (TRUE == cr_BMatrix_operator_bracket(Mat, r, c)) //-- if is bit
            {
              return r + 1;
            }
        }
    }
  return 0;
}



//----------------------------------------------------------------------
// cr_pgh_print
//
// Prints content of matrix when debug is on
// in Mat: Pointer to struct cr_BMatrixType
// in debug: Flag indicating whether matrix shall be printed
// return -

//-- void print(BMatrixType &M, bool debug);
void cr_pgh_print(struct cr_BMatrixType *Mat, gboolean debug)
{
  if (debug)
    {
      g_print("cr_pgh_print, Mat->rows: %d, Mat->colsInBits: %d, Mat->buckets: %d\n", Mat->rows, Mat->colsInBits,
              Mat->buckets);
      //-- M->Print();
      cr_BMatrix_Print(Mat);
    }
}


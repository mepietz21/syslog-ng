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
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cr_pi_types.h"
#include "cr_pi_verifier.h"
#include "cr_randomstuff.h"
#include "cr_plain_gauss_helper.h"
#include "cr_matrix.h"
#include "messages.h"

//
//-- cr_B256 -----
//

// Function to set a bit at a given position
void cr_B256_setBit(struct cr_B256 *self, guint position)
{
  g_assert(position < 256);
  const guint memberIndex = position >> 6;
  const guint bitIndexInMember = 63 - (position & 63);
  self->data.parts[memberIndex] |= (1ULL << bitIndexInMember);
}


gboolean cr_B256_getBit(struct cr_B256 *self, guint position)
{
  g_assert(position < 256);
  const guint memberIndex = position >> 6; //-- Which uint64_t to use (0-3)
  const guint bitIndexInMember = 63 - (position & 63); //-- Which bit in that uint64_t
  return (self->data.parts[memberIndex] >> bitIndexInMember) & 1;
}



#if 0
struct cr_B256 cr_B256_operatorXOR(struct cr_B256 *self, const struct cr_B256 *other)
{
  struct cr_B256 result;
  result.x = self->x ^ other->x;
  result.y = self->y ^ other->y;
  result.z = self->z ^ other->z;
  result.w = self->w ^ other->w;
  return result;
}
#endif

#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
__attribute__((target("avx2")))
#endif
struct cr_B256 cr_B256_operatorXOR(const struct cr_B256 *self, const struct cr_B256 *other)
{
  struct cr_B256 result;

#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
  //-- AVX2 implementation
  //   NOTE: Input buffers must provide 32-bit-aligned memory.
  //   This makes AVX2 a bit faster than SSE2.
  //   This program will COREDUMP when unaligned memory is provided, because
  //   load/store instead loadu/storeu functions are used!
  __m256i a = _mm256_load_si256((const __m256i *)self);
  __m256i b = _mm256_load_si256((const __m256i *)other);
  __m256i result_vec = _mm256_xor_si256(a, b);
  _mm256_store_si256((__m256i *)&result, result_vec);

#elif defined(CPU_SSE2) && (CPU_SSE2 == 1)
  //-- SSE2 implementation
  const __m128i *a = (const __m128i *)self;
  const __m128i *b = (const __m128i *)other;
  __m128i *res = (__m128i *)&result;
  res[0] = _mm_xor_si128(_mm_loadu_si128(&a[0]), _mm_loadu_si128(&b[0]));
  res[1] = _mm_xor_si128(_mm_loadu_si128(&a[1]), _mm_loadu_si128(&b[1]));

#else // Fallback to the scalar (CPU_OTHER) version
  //-- No special CPU functionality. Scalar (CPU_OTHER) implementation
  for (int i = 0; i < 4; ++i)
    {
      result.data.parts[i] = self->data.parts[i] ^ other->data.parts[i];
    }
#endif

  return result;
}



//-- dbg / test helper
void cr_B256_print(const struct cr_B256 *self, const char *name)
{
  g_print("%s\n", name);
  cr_print_num_dhb("x", self->data.coords.x);
  cr_print_num_dhb("y", self->data.coords.y);
  cr_print_num_dhb("z", self->data.coords.z);
  cr_print_num_dhb("w", self->data.coords.w);
}



//-- dbg /test helper
void cr_printBinaryWithPadding(const uint64_t value)
{
  for (int i = sizeof(uint64_t) * 8 - 1; i >= 0; i--)
    {
      g_print("%ld", (value >> i) & 1);
      if (i % 8 == 0)
        g_print(" "); //-- Group by 8 bits for readability
    }
}



//-- dbg / test helper
void cr_print_num_dhb(const char *name, const uint64_t value)
{
  g_print("%10s: %20lu, 0x%016" PRIX64 ", bin ", name, value, value);
  cr_printBinaryWithPadding(value);
  g_print("\n");
}



//
//-- cr_BMatrixType -----
//



//----------------------------------------------------------------------
// cr_BMatrix_operator_bracket (getBit)
// Replacement of c++ bracket operator.
// Get bit state of table entry at given position
// in self: Pointer to struct cr_BMatrixType containing table
// in row: table row
// in col: table col
// return boolean bit status

gboolean cr_BMatrix_operator_bracket(struct cr_BMatrixType *self, const gsize row, const gsize col)
{
  //         bool operator()(const int row, const int col) const {
  //              return data[row * buckets + (col / B_B_BITS )].getBit(col%B_B_BITS);
  //          }
  g_assert(row < self->rows);
  g_assert(col < self->colsInBits);
  struct cr_B256 *p = &self->data[row * self->buckets + (col >> 8)];
  const guint position = col & 255;
  const guint memberIndex = position >> 6; //-- Which uint64_t to use (0-3)
  const guint bitIndexInMember = 63 - (position & 63); //-- Which bit in that uint64_t
  return (p->data.parts[memberIndex] >> bitIndexInMember) & 1;
}



//----------------------------------------------------------------------
// cr_BMatrix_setBit
// Sets bit in internal data struct fnt_B256 table specified by row and col
// in self:Pointer to struct cr_BMatrixType
// in row
// in col
// returns -

void cr_BMatrix_setBit(struct cr_BMatrixType *self, const gsize row, const gsize col)
{
  //-- data[row * (buckets) + (col / B_B_BITS)].setBit(col%B_B_BITS);
  g_assert(col < self->colsInBits);
  struct cr_B256 *p = &self->data[row * self->buckets + (col >> 8)];
  const guint position = col & 255;
  const guint memberIndex = position >> 6;      // Which uint64_t (0-3)
  const guint bitIndexInMember = 63 - (position & 63); // Bit position (63 is MSB)
  p->data.parts[memberIndex] |= 1ULL << bitIndexInMember;
}



//----------------------------------------------------------------------
// cr_BMatrix_SetCustomDataPointer
// Links table to data and frees it before assignment if needed
// in self: Pointer to struct cr_BMatrixType containing data pointer
// in data: Pointer to struct cr_B256 table managed by caller
// return -

void cr_BMatrix_SetCustomDataPointer(struct cr_BMatrixType *self,
                                     struct cr_B256 *data)
{
  if (self->freeableData)
    {
      free(self->data);
      self->data = NULL;
    }
  self->data = data;
  self->freeableData = FALSE;
}



//----------------------------------------------------------------------
// cr_BMatrix_swapRows
// Replacement of c++ std::swap for table entries
// in a: Pointer to struct cr_B256
// in b: Pointer to struct cr_B256
// return -

void cr_swapB256(struct cr_B256 *a, struct cr_B256 *b)
{
  struct cr_B256 temp;
  memcpy(&temp, a, sizeof(struct cr_B256));
  memcpy(a, b, sizeof(struct cr_B256));
  memcpy(b, &temp, sizeof(struct cr_B256));
}



//----------------------------------------------------------------------
// cr_BMatrix_swapRows
// Swap table rows
// in self: Pointer to initialized struct cr_BMatrixType
// in l
// in k
// return -

void cr_BMatrix_swapRows(struct cr_BMatrixType *self, gsize l, gsize k)
{
  for (gsize i = 0; i < self->buckets; ++i)
    {
      cr_swapB256(&self->data[l * self->buckets + i], &self->data[k * self->buckets + i]);
    }
}



//----------------------------------------------------------------------
// cr_BMatrix_Print
// Prints Initializes given structure
// in self: Pointer to initialized struct cr_BMatrixType

void cr_BMatrix_Print(struct cr_BMatrixType *self)
{
  gboolean isbit;
  for (gsize row = 0; row < self->rows; ++row)
    {
      for (gsize col = 0; col < self->colsInBits; ++col)
        {
          // std::cout << (int)(*this)(row, col) << " ";
          isbit = cr_BMatrix_operator_bracket(self, row, col);
          g_print("%d ", (int) isbit);
        }
      g_print("\n"); // std::cout << std::endl;
    }
  g_print("\n"); // std::cout << std::endl;
}



//----------------------------------------------------------------------
// cr_BMatrix_ctor_static
//
// Initializes given structure
// in self: Pointer to allocated but uninitialized struct cr_BMatrixType
// in m: rows (for internal struct cr_B256 table)
// in n: colsInBits (for internal struct cr_B256 table)
//
// In C this function has to be called manually to initialize
// struct cr_BMatrixType. Used in cr_BMatrix_I.

void cr_BMatrix_ctor_static(struct cr_BMatrixType *self, gsize m, gsize n)
{
  self->rows = m;
  self->colsInBits = n;
  double temp_m = ceil(self->colsInBits / (double)(B_B_BITS));
  gsize int32ColCount = (gsize) temp_m;
  self->buckets = int32ColCount;
  gsize n_elements = self->rows * self->buckets;
  gsize total_bytes = n_elements * sizeof(struct cr_B256);
  self->data = (struct cr_B256 *)aligned_alloc(AVX2_ALIGNMENT, total_bytes); //-- 32 == AVX2_ALIGNMENT
  if (NULL == self->data)
    {
      // fixed: msg_error
      msg_error("Failed to allocate BMatrix memory");
      return;
    }
  memset(self->data, 0, total_bytes);
  self->freeableData = TRUE;
}



//----------------------------------------------------------------------
// cr_BMatrix_destructor_static
//
// Frees table pointed to by self->data
// In C this function has to be called manually on cleanup
// in self: Pointer to allocated and initialized struct cr_BMatrixType
// return -

void cr_BMatrix_destructor_static(struct cr_BMatrixType *self)
{
  if (self->freeableData)
    {
      free(self->data);
    }
  self->data = NULL;
}



//----------------------------------------------------------------------
// cr_BMatrix_ctor_dyn
//
// Creates and provides one fully allocated and initialized matrix
// struct cr_BMatrixType
//
// The caller is responsible to free allocated data!
// in size: used as m and n in cr_BMatrix_ctor
// returns Pointer to heap allocated struct cr_BMatrixType

struct cr_BMatrixType *cr_BMatrix_ctor_dyn(gsize m, gsize n)
{
  struct cr_BMatrixType *mat = (struct cr_BMatrixType *) g_malloc0(sizeof(struct cr_BMatrixType));
  cr_BMatrix_ctor_static(mat, m, n);
  return mat;
}



//----------------------------------------------------------------------
// cr_BMatrix_destructor_dyn
//
// Frees all data of *self
// In C this function has to be called manually on cleanup
// in self: Pointer-Pointer to  struct cr_BMatrixType
// return -

void cr_BMatrix_destructor_dyn(struct cr_BMatrixType **self)
{
  if ((*self)->freeableData)
    {
      free((*self)->data);
    }
  (*self)->data = NULL;
  g_free(*self);
  *self = NULL;
}



//----------------------------------------------------------------------
// cr_BMatrix_I
// Creates and provides one fully allocated and initialized identity matrix
// struct cr_BMatrixType
// It is a square matrix with ones on the main diagonal and zeros everywhere else.
//
// The caller is responsible to free allocated data!
// in size: used as m and n in cr_BMatrix_ctor
// returns Pointer to heap allocated struct cr_BMatrixType

struct cr_BMatrixType *cr_BMatrix_I(gsize size)
{
  //-- BMatrixType* mat = new BMatrixType(size, size);
  struct cr_BMatrixType *mat = (struct cr_BMatrixType *) g_malloc0(sizeof(struct cr_BMatrixType));
  cr_BMatrix_ctor_static(mat, size, size);

  for (gsize i = 0; i < size; ++i)
    {
      //-- mat->setBit(i, i);
      cr_BMatrix_setBit(mat, i, i);
    }
  return mat;
}






//
//-- cr_MatrixType -----
//



//----------------------------------------------------------------------
// cr_Matrix_SetCustomDataPointer
// Links table to data and frees it before assignment if needed
// in self: Pointer to struct cr_MatrixType containing data pointer
// in data: Pointer to guint32 data managed by caller
// return -

void cr_Matrix_SetCustomDataPointer(struct cr_MatrixType *self, guint32 *data)
{
  if (self->freeableData)
    {
      g_free(self->data);
      self->data = NULL;
    }
  self->data = data;
  self->freeableData = FALSE;
}



//----------------------------------------------------------------------
// cr_Matrix_operator_bracket
// Replacement of c++ bracket operator
// Get bit state of table entry at given position
// in self: Pointer to struct cr_MatrixType containing table
// in row: table row
// in col: table col
// return boolean bit status

gboolean cr_Matrix_operator_bracket(struct cr_MatrixType *self, const gsize row, const gsize col)
{
  gboolean ret;
  gsize i = row * self->buckets + col / B_BITS;
  gsize offset = B_BITS - 1 - (col % B_BITS);
  if (0 != ((self->data[i] >> offset) & 1))
    ret = TRUE;
  else
    ret = FALSE;

  return ret;
}


//----------------------------------------------------------------------
// cr_swap_unsigned_int
// Replacement of c++ std::swap for table entries
// in a: Pointer to unsigned int
// in b: Pointer to unsigned int
// return -

void cr_swap_unsigned_int(guint32 *a, guint32 *b)
{
  guint32 temp;
  memcpy(&temp, a, sizeof(guint32));
  memcpy(a, b, sizeof(guint32));
  memcpy(b, &temp, sizeof(guint32));
}



//----------------------------------------------------------------------
// cr_Matrix_swapRows
// Swap the rows by swapping the corresponding elements in the data vector
// in self: Pointer to struct cr_MatrixType
// in l:
// in k:
// return -

void cr_Matrix_swapRows(struct cr_MatrixType *self, gsize l, gsize k)
{
  for (gsize i = 0; i < self->buckets; ++i)
    {
      cr_swap_unsigned_int(&self->data[l * self->buckets + i], &self->data[k * self->buckets + i]);
    }
}



//----------------------------------------------------------------------
// cr_Matrix_Print
// print the matrix
// in self: Pointer to struct cr_MatrixType
// return -

void cr_Matrix_Print(struct cr_MatrixType *self)
{
  gboolean isbit;
  for (gsize i = 0; i < self->rows; ++i)
    {
      for (gsize j = 0; j < self->colsInBits; ++j)
        {
          //-- std::cout << (int)(*this)(i, j);
          isbit = cr_Matrix_operator_bracket(self, i, j);

          if ((j + 1) % 8 == 0)
            g_print("%d ", (int) isbit);
          else
            g_print("%d", (int) isbit);

          /*for (int b = B_BITS-1; b >= (ceil(matrix.colsInBits/B_BITS) == j ? B_BITS - (matrix.colsInBits%B_BITS):0); --b) {
              //int bit = (matrix.data[i * matrix.buckets + j] >> b) & 1;
              int bit = (int)matrix(i, j);
              std::cout << (int)matrix(i, j);
          }*/
          //std::cout << ' ';// << matrix.data[i * matrix.buckets + j] << ' ';
        }
      g_print("\n");
    }
  g_print("\n");
}



//----------------------------------------------------------------------
// cr_Matrix_ctor
// Initializes given structure
// in self: Pointer to allocated but uninitialized struct cr_MatrixType
// in m: rows (for internal data table)
// in n: colsInBits (for internal data table)
//
// In C this function has to be called manually to initialize
// struct cr_MatrixType. Used in cr_Matrix_I.

void cr_Matrix_ctor(struct cr_MatrixType *self, gsize m, gsize n)
{
  self->rows = m;
  self->colsInBits = n;
  double temp_m = ceil((double)self->colsInBits / (double)(B_BITS));
  gsize int32ColCount = (gsize) temp_m;
  self->buckets = int32ColCount;
  self->data = (guint32 *) g_malloc0(sizeof(guint32) * self->rows * self->buckets);
  self->freeableData = TRUE;
}



//----------------------------------------------------------------------
// Frees data pointed to by self->data
// In C this function has to be called manually on cleanup
// in self: Pointer to allocated and initialized struct cr_MatrixType
// return -

void cr_Matrix_destructor(struct cr_MatrixType *self)
{
  if (self->freeableData)
    {
      g_free(self->data);
    }
  self->data = NULL;
}



//----------------------------------------------------------------------
// cr_Matrix_toggle
// Toggle bit by XOR
// in self: Pointer to allocated and initialized struct cr_MatrixType
// in row:
// in col:
// return -

void cr_Matrix_toggle(struct cr_MatrixType *self, const gsize row, const gsize col)
{
#if 0
  gsize i = row * self->buckets + col / B_BITS;
  gsize offset = B_BITS - 1 - (col % B_BITS);
  self->data[i] ^= (1u << offset);
#endif
  gsize i = row * self->buckets + (col >> B_BITS_SHIFT);  //-- B_BITS_SHIFT == 5
  gsize offset = B_BITS - 1 - (col & (B_BITS - 1));
  self->data[i] ^= (1u << offset);
}



//----------------------------------------------------------------------
// cr_Matrix_Create
// Alternative constructor as function
// Allocates and initializes instance of type cr_MatrixType
// The caller is responsible of allocated memory.
// Note: This function was not a member function of the C++ struct cr_MatrixType
// in m: rows (for internal data table)
// in n: colsInBits (for internal data table)
// return pointer to instance of type struct cr_MatrixType

struct cr_MatrixType *cr_Matrix_Create(gsize m, gsize n)
{
  gsize rows = m;
  gsize cols = n;
  //-- MatrixType *matrix = new MatrixType;
  struct cr_MatrixType *matrix = (struct cr_MatrixType *) g_malloc0(sizeof(struct cr_MatrixType));
  matrix->rows = rows;
  matrix->colsInBits = cols;
  double temp_m = ceil((double)cols / (double)(B_BITS));
  gsize int32ColCount = (gsize) temp_m;
  matrix->buckets = int32ColCount;
  //-- matrix->data = new guint32 [rows * int32ColCount]();
  matrix->data = (guint32 *) g_malloc0(sizeof(guint32) * matrix->rows * matrix->buckets);
  matrix->freeableData = TRUE;
  return matrix;
}



//----------------------------------------------------------------------
// cr_Matrix_I
// Allocates memory and toggles bits
// The caller is responsible of allocated memory.
// in size: used for row and col of internal data
// return pointer to instance of type struct cr_MatrixType

struct cr_MatrixType *cr_Matrix_I(gsize size)
{
  struct cr_MatrixType *I = cr_Matrix_Create(size, size);
  for (gsize i = 0; i < size; ++i)
    {
      //-- (*I).toggle(i, i);
      cr_Matrix_toggle(I, i, i);
    }
  return I;
}



//----------------------------------------------------------------------
// cr_Matrix_FillWithRandomness
//
// in m: Pointer to instance of type struct cr_MatrixType
// in k: THE_K which is defined to 5 (count of Random numbers in DRN)
// return -

void cr_Matrix_FillWithRandomness(struct cr_MatrixType *m, int k) //-- int k = 5
{
  for (gsize c = 0; c < m->colsInBits; c++)
    {
      gsize *randoms = cr_distinctRandomEz((gsize)(m->rows - 1), k, (gint)c);
      // Iterate through the column c
      for (int i = 0; i < k; ++i)
        {
          gsize row = randoms[i];
          // guint32 bitmask = (1 << (B_BITS - (c % B_BITS + 1)));
          // set this to a 1
          //-- m->togle(row, c);// data[row * (m->buckets) + (c / B_BITS)] ^= bitmask;
          cr_Matrix_toggle(m, row, c);
        }
      //-- delete [] randoms;
      g_free(randoms);
    }
}


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


#ifndef cr_matrix_h
#define cr_matrix_h

#include <glib.h>
#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* ---------------------------------------------------------------------
 * Code from THESIS expects ulong4. Code can not be taken over directly!!
 * --> 64 bit integers must be used explicite.
 * int might be the same as long, namely 32 bit integers.
 * THESIS was written for Apple macOS, using CPP-Metal, Swift, Xcode, CUDA
 * For conversion from C++ to C:
 * - structs contain only data. Memberfunctions become
 *   normal functions with a self pointer to 'their' struct.
 * - in some cases instead of int uint64_t is needed
 * ---------------------------------------------------------------------
 */


#define BITS_PER_BYTE 8
#define BYTES 4 // 4 for guint32
#define B_BITS (BITS_PER_BYTE * BYTES)
#define B_B_BITS 256
#define B_B_BIT_SHIFT 8 //-- log2(256) is 8, so N / 256 == N >> 8
#define B_BITS_SHIFT  5 //-- log2(32) is 5, so N / 32 == N >> 5

struct cr_B256
{
  union
  {
    struct
    {
      uint64_t x;
      uint64_t y;
      uint64_t z;
      uint64_t w;
    } coords; // Added name 'coords'
    uint64_t parts[4];
  } data; // Added name 'data'
} __attribute__((aligned(32)));

// Metal-Shading-Language-Specification.pdf, Table 2.2. Size and alignment of scalar data type
// unsigned long -> uint64_t, 8 Bytes

// Function to set a bit at a given position
void cr_B256_setBit(struct cr_B256 *self, guint position);

// Function to get a bit at a given position
gboolean cr_B256_getBit(struct cr_B256 *self, guint position);

// data = self XOR other
//struct cr_B256 cr_B256_operatorXOR(struct cr_B256 *self, const struct cr_B256 *other);
struct cr_B256 cr_B256_operatorXOR(const struct cr_B256 *self, const struct cr_B256 *other);

//-- test and debug helper:
void cr_B256_print(const struct cr_B256 *self, const char *name);
void cr_printBinaryWithPadding(const uint64_t value);
void cr_print_num_dhb(const char *name, const uint64_t value);



//-- THESIS c++ namespace Matrix { struct BMatrixTyp { ...

struct cr_BMatrixType
{
  struct cr_B256 *data;
  gsize rows;
  gsize buckets;
  gsize colsInBits;
  gboolean freeableData; //-- private c++ member
};

gboolean cr_BMatrix_operator_bracket(struct cr_BMatrixType *self, const gsize row, const gsize col);
void cr_BMatrix_SetCustomDataPointer(struct cr_BMatrixType *self, struct cr_B256 *data);
void cr_swapB256(struct cr_B256 *a, struct cr_B256 *b); //-- std::swap replacemnt in C
void cr_BMatrix_swapRows(struct cr_BMatrixType *self, gsize l, gsize k);
void cr_BMatrix_Print(struct cr_BMatrixType *self);
void cr_BMatrix_ctor_static(struct cr_BMatrixType *self, gsize m, gsize n);
struct cr_BMatrixType *cr_BMatrix_ctor_dyn(gsize m, gsize n);
void cr_BMatrix_destructor_static(struct cr_BMatrixType *self);
void cr_BMatrix_destructor_dyn(struct cr_BMatrixType **self);
struct cr_BMatrixType *cr_BMatrix_I(gsize size);
void cr_BMatrix_setBit(struct cr_BMatrixType *self, const gsize row, const gsize col);



//-- THESIS c++ mamespace Matrix { struct MatrixTyp { ...

struct cr_MatrixType
{
  guint32 *data;
  gsize rows;
  gsize buckets;
  gsize colsInBits;
  gboolean freeableData;
};

// Metal-Shading-Language-Specification.pdf, Table 2.2. Size and alignment of scalar data type
// unsigned int -> guint32, 4 Bytes


void cr_Matrix_SetCustomDataPointer(struct cr_MatrixType *self, guint32 *data);
gboolean cr_Matrix_operator_bracket(struct cr_MatrixType *self, const gsize row, const gsize col);
void cr_Matrix_swapRows(struct cr_MatrixType *self, gsize l, gsize k);
void cr_Matrix_Print(struct cr_MatrixType *self);
void cr_Matrix_ctor(struct cr_MatrixType *self, gsize m, gsize n);
void cr_Matrix_destructor(struct cr_MatrixType *self);
void cr_Matrix_toggle(struct cr_MatrixType *self, const gsize row, const gsize col);
void cr_swap_unsigned_int(guint32 *a, guint32 *b);

//-- non-member functions of MatrixType (outside c++ class / struct)
struct cr_MatrixType *cr_Matrix_Create(gsize m, gsize n);
struct cr_MatrixType *cr_Matrix_I(gsize size);
void cr_Matrix_FillWithRandomness(struct cr_MatrixType *m, int k); //-- int k = 5

#endif /* cr_matrix_h */

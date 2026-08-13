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


#ifndef cr_plain_gauss_helper_h
#define cr_plain_gauss_helper_h

#include <stdio.h>
#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cr_pi_types.h"
#include "cr_matrix.h"


//-- The standalone implementation uses a build setup where configure.ac
//   prepares an architecture parameter for configure.
//   Because here we are in the syslog-ng environment, its better not to
//   touch configure.ac and provide the preprocessors for CPU optimization
//   here.

//-- CPU Preprocessors (One and only one must be set to 1 and the others must be set to 0)
//   CPU_AVX2: Intel x86_64 CPU with AVX2 support (fastest, not available on all Intel CPUs)
//   CPU_SSE2: Intel x86_64 CPU with SSE2 support (fast, available on most Intel CPUs, and nearly as fast as CPU_AVX2)
//   CPU_OTHER: Older Intel CPUs or ARM CPUs like Raspberry Pi

#if defined(__x86_64__)
#define CPU_AVX2  0
#define CPU_SSE2  1
#define CPU_OTHER 0
#else
#define CPU_AVX2  0
#define CPU_SSE2  0
#define CPU_OTHER 1
//-- TODO optimization for ARM64 Neon #include <arm_neon.h>
#endif

//-- include specific header and check configuration
#if defined(CPU_AVX2) && (CPU_AVX2 == 1)
#include <immintrin.h> //-- Header for AVX2 intrinsics, current CPU
//NOTE:  aligned memory MUST BE USED where _mm256_loadu_si256 and _mm256_storeu_si256 functions is used
#ifndef __AVX2__
#error "Logic Error: CPU_AVX2 is enabled in headers, but -mavx2 flag is missing in compiler settings!"
#endif
#endif

#if defined(CPU_SSE2) && (CPU_SSE2 == 1)
#include <emmintrin.h> //-- Header for SSE2 intrinsics, older CPU
#ifndef __SSE2__
#error "Logic Error: CPU_SSE2 enabled in header, but -msse2 flag is missing!"
#endif
#endif

#define AVX2_ALIGNMENT 32 //-- used also for other CPU cfgs

//-- XOR depending on CPU configuration
void xor_buffers(void *buf_a, const void *buf_b, gsize len);

//-- helper: ctor of std::vector<cr_XOR_TYPE> C replacement
GPtrArray *create_GPtrArray_cr_XOR_TYPE(gsize count, GError **error);

//-- helper: destructor of std::vector<cr_XOR_TYPE> C replacement
void free_GPtrArray_cr_XOR_TYPE(GPtrArray **gpa);

//-- helper to write time consumption into protocol file
void add_to_protocol_timediff(cr_VerifierContext *ctx, uint64_t ndiff, const char *szText);

//-- std::vector<PI::XOR_TYPE> Solve(BMatrixType *M, std::vector<PI::XOR_TYPE> &v, bool debug = false);
GPtrArray *cr_pgh_Solve(struct cr_BMatrixType *Mat, GPtrArray *gpa, gboolean debug, cr_VerifierContext *ctx);

//-- int Pivot(BMatrixType *M, int &currentRow, int &currentCol);
int cr_pgh_Pivot(struct cr_BMatrixType *Mat, int currentRow, int currentCol);

//-- void ForwardReduction(BMatrixType *M, BMatrixType *I);
void cr_pgh_ForwardReduction(struct cr_BMatrixType *Mat, struct cr_BMatrixType *Imat);

//-- std::vector<PI::XOR_TYPE> ApplyBookkeeping(BMatrixType &I, std::vector<PI::XOR_TYPE> &v);
GPtrArray *cr_pgh_ApplyBookkeeping(struct cr_BMatrixType *Imat, GPtrArray *gpa, GError **error);

//-- std::vector<PI::XOR_TYPE> BackSubstitution(BMatrixType &M, std::vector<PI::XOR_TYPE> &v);
GPtrArray *cr_pgh_BackSubstitution(struct cr_BMatrixType *Mat, GPtrArray *gpa, GError **error);

//-- int RankOf(BMatrixType &m);
int cr_pgh_RankOf(struct cr_BMatrixType *Mat);

//-- void print(BMatrixType &M, bool debug);
void cr_pgh_print(struct cr_BMatrixType *Mat, gboolean debug);

#endif /* cr_plain_gauss_helper_h */

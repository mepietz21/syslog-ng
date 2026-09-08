/*
* Copyright (c) 2026 Airbus Commercial Aircraft
* Unit Tests for syslog-ng secure-logging crash recovery verifier & matrix helpers
* Framework: Criterion
*/
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <glib.h>
#include <string.h>
#include <stdint.h>
#include "cr_pi_types.h"
#include "cr_pi_shared.h"
#include "cr_matrix.h"
#include "cr_pi_verifier.h"
#include "cr_plain_gauss_helper.h"
/*
======================================================================
===
* TEST SUITE: Bit-256 & Bit-Matrix Operations (cr_matrix.c)
*
======================================================================
=== */
Test(cr_matrix, b256_bit_setting_and_xor)
{
struct cr_B256 a, b, res;
memset(&a, 0, sizeof(a));
memset(&b, 0, sizeof(b));
/* Bits an bestimmten Positionen setzen */
cr_B256_setBit(&a, 0); /* Bit 0 in part 0 */
cr_B256_setBit(&a, 127); /* Bit in part 1 */
cr_B256_setBit(&b, 127);
cr_B256_setBit(&b, 255); /* Bit in part 3 */
cr_assert_eq(cr_B256_getBit(&a, 0), TRUE, "Bit 0 in 'a' should be set");
cr_assert_eq(cr_B256_getBit(&a, 127), TRUE, "Bit 127 in 'a' should be set");
cr_assert_eq(cr_B256_getBit(&a, 10), FALSE, "Bit 10 in 'a' should NOT be set");
/* XOR-Operation testen */
res = cr_B256_operatorXOR(&a, &b);
cr_assert_eq(cr_B256_getBit(&res, 0), TRUE, "Bit 0 should be 1 after XOR (1 ^ 0)");
cr_assert_eq(cr_B256_getBit(&res, 127), FALSE, "Bit 127 should be 0 after XOR (1 ^ 1)");
cr_assert_eq(cr_B256_getBit(&res, 255), TRUE, "Bit 255 should be 1 after XOR (0 ^ 1)");

}
Test(cr_matrix, bmatrix_identity_and_swap)
{
gsize size = 8;
struct cr_BMatrixType *mat = cr_BMatrix_I(size);
cr_assert_not_null(mat, "cr_BMatrix_I returned NULL");
/* Prüfen, ob die Hauptdiagonale 1 ist und sonst 0 */
for (gsize r = 0; r < size; ++r)
{
for (gsize c = 0; c < size; ++c)
{
gboolean expected = (r == c) ? TRUE : FALSE;
gboolean actual = cr_BMatrix_operator_bracket(mat, r, c);
cr_assert_eq(actual, expected, "Matrix bit at (%zu, %zu) incorrect", r, c);
}
}
/* Zeilen vertauschen (Zeile 0 und Zeile 1) */
cr_BMatrix_swapRows(mat, 0, 1);
cr_assert_eq(cr_BMatrix_operator_bracket(mat, 0, 0), FALSE);
cr_assert_eq(cr_BMatrix_operator_bracket(mat, 0, 1), TRUE);
cr_assert_eq(cr_BMatrix_operator_bracket(mat, 1, 0), TRUE);
cr_assert_eq(cr_BMatrix_operator_bracket(mat, 1, 1), FALSE);
cr_BMatrix_destructor_dyn(&mat);
cr_assert_null(mat, "Pointer should be reset to NULL after destructor_dyn");
}
Test(cr_matrix, matrix_toggle_and_bits)
{
struct cr_MatrixType *m = cr_Matrix_Create(4, 4);
cr_assert_not_null(m);
cr_assert_eq(cr_Matrix_operator_bracket(m, 2, 2), FALSE);
cr_Matrix_toggle(m, 2, 2);
cr_assert_eq(cr_Matrix_operator_bracket(m, 2, 2), TRUE);
cr_Matrix_toggle(m, 2, 2);
cr_assert_eq(cr_Matrix_operator_bracket(m, 2, 2), FALSE);
cr_Matrix_destructor(m);
g_free(m);
}
/*
======================================================================
===

* TEST SUITE: Gaussian Elimination & Vector XOR (cr_plain_gauss_helper.c)
*
======================================================================
=== */
Test(cr_gauss, xor_buffers_aligned)
{
/* Allocating 32-byte aligned buffers for SIMD / AVX2 / SSE2 compatibility */
gsize len = 64;
uint8_t *buf_a = aligned_alloc(AVX2_ALIGNMENT, len);
uint8_t *buf_b = aligned_alloc(AVX2_ALIGNMENT, len);
memset(buf_a, 0xAA, len); /* 10101010 */
memset(buf_b, 0x55, len); /* 01010101 */
xor_buffers(buf_a, buf_b, len);
/* 0xAA ^ 0x55 == 0xFF */
for (gsize i = 0; i < len; ++i)
{
cr_assert_eq(buf_a[i], 0xFF, "Mismatch at byte %zu after xor_buffers", i);
}
free(buf_a);
free(buf_b);
}
Test(cr_gauss, create_and_free_gptrarray_xor)
{
GError *error = NULL;
gsize count = 5;
GPtrArray *gpa = create_GPtrArray_cr_XOR_TYPE(count, &error);
cr_assert_null(error, "Error set during allocation: %s", error ? error->message : "");
cr_assert_not_null(gpa, "gpa should not be NULL");
cr_assert_eq(gpa->len, count, "Expected array length %zu", count);
/* Verify elements are aligned */
cr_assert_not_null(gpa->pdata[0]);
cr_assert_eq(((uintptr_t)gpa->pdata[0]) % AVX2_ALIGNMENT, 0, "Data block is not 32-byte aligned!");
free_GPtrArray_cr_XOR_TYPE(&gpa);
cr_assert_null(gpa, "gpa pointer must be set to NULL after free");
}

/*
======================================================================
===
* TEST SUITE: Verifier Helpers & Hash Functions (cr_pi_verifier.c)
*
======================================================================
=== */
Test(cr_verifier_helpers, fnv1a_hash_and_id_equal)
{
cr_ID_TYPE id1 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
cr_ID_TYPE id2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
cr_ID_TYPE id3 = {0xFF, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

guint hash1 = fnv1a_hash_ID_LEN(id1);
guint hash2 = fnv1a_hash_ID_LEN(id2);
guint hash3 = fnv1a_hash_ID_LEN(id3);
cr_assert_eq(hash1, hash2, "Hashes for identical IDs should match");
cr_assert_neq(hash1, hash3, "Hashes for different IDs should not match");
cr_assert_eq(id_type_buffer_equal(id1, id2), TRUE);
cr_assert_eq(id_type_buffer_equal(id1, id3), FALSE);
}
Test(cr_verifier_helpers, is_equal_nullvector_check)
{
unsigned char zero_buf[32] = {0};
unsigned char non_zero_buf[32] = {0};
non_zero_buf[15] = 0x01;
cr_assert_eq(is_equal_nullvector(zero_buf, sizeof(zero_buf), FALSE), TRUE);
cr_assert_eq(is_equal_nullvector(non_zero_buf, sizeof(non_zero_buf), FALSE), FALSE);
cr_assert_eq(is_equal_nullvector(NULL, 32, FALSE), TRUE, "NULL buffer should be treated as nullvector");
}
Test(cr_verifier_helpers, cpu_configuration_and_info)
{
gboolean valid_cfg = cr_check_cpu_cfg();
cr_assert_eq(valid_cfg, TRUE, "Exactly one CPU configuration preprocessor must be defined as 1");
GString *info = get_cpu_config_info(FALSE);
cr_assert_not_null(info);

cr_assert_gt(info->len, 0, "CPU config info string should not be empty");
g_string_free(info, TRUE);
}
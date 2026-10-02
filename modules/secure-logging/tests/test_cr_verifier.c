/*
 * Copyright (c) 2024 Gergo Ferenc Kovacs
 * Copyright (c) 2026 Airbus Commercial Aircraft
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

#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
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
    cr_B256_setBit(&a, 0);
    cr_B256_setBit(&a, 63);
    cr_B256_setBit(&a, 127);
    cr_B256_setBit(&a, 255);
    cr_B256_setBit(&b, 63);
    cr_B256_setBit(&b, 127);
    cr_B256_setBit(&b, 128);
    cr_assert_eq(cr_B256_getBit(&a, 0), TRUE, "Bit 0 in 'a' should be set");
    cr_assert_eq(cr_B256_getBit(&a, 127), TRUE, "Bit 127 in 'a' should be set");
    cr_assert_eq(cr_B256_getBit(&a, 255), TRUE, "Bit 255 in 'a' should be set");
    cr_assert_eq(cr_B256_getBit(&a, 10), FALSE, "Bit 10 in 'a' should NOT be set");
    /* XOR-Operation testen */
    res = cr_B256_operatorXOR(&a, &b);
    cr_assert_eq(cr_B256_getBit(&res, 0), TRUE, "Bit 0 should be 1 after XOR (1 ^ 0)");
    cr_assert_eq(cr_B256_getBit(&res, 127), FALSE, "Bit 127 should be 0 after XOR (1 ^ 1)");
    cr_assert_eq(cr_B256_getBit(&res, 128), TRUE, "Bit 128 should be 1 after XOR (0 ^ 1)");
    cr_assert_eq(cr_B256_getBit(&res, 255), TRUE, "Bit 255 should be 1 after XOR (1 ^ 0)");
}

Test(cr_matrix, bmatrix_identity_and_swap)
{
    gsize size = 16;
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
    cr_assert_not_null(buf_a, "Could not allocate aligned left operand");
    cr_assert_not_null(buf_b, "Could not allocate aligned right operand");
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

Test(cr_gauss, rank_of_known_matrices)
{
    struct cr_BMatrixType *matrix = cr_BMatrix_ctor_dyn(4, 4);
    cr_assert_not_null(matrix);
    cr_assert_eq(cr_pgh_RankOf(matrix), 0, "Zero matrix must have rank 0");

    cr_BMatrix_setBit(matrix, 0, 0);
    cr_assert_eq(cr_pgh_RankOf(matrix), 1, "One independent row must have rank 1");

    cr_BMatrix_setBit(matrix, 1, 1);
    cr_BMatrix_setBit(matrix, 2, 2);
    cr_assert_eq(cr_pgh_RankOf(matrix), 3, "Three independent rows must have rank 3");

    cr_BMatrix_setBit(matrix, 3, 3);
    cr_assert_eq(cr_pgh_RankOf(matrix), 4, "Identity matrix must have full rank");
    cr_BMatrix_destructor_dyn(&matrix);
    cr_assert_null(matrix);
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
    for (gsize i = 0; i < count; ++i)
    {
        cr_assert_not_null(gpa->pdata[i]);
        cr_assert_eq(((uintptr_t)gpa->pdata[i]) % AVX2_ALIGNMENT, 0,
                     "Data block %zu is not 32-byte aligned!", i);
    }
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
    cr_assert_eq(hash1, hash2, "Hashes for identical IDs should match");
    cr_assert_neq(hash1, 0, "Hash must not be the uninitialized zero value");
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

Test(cr_verifier_helpers, read_master_key_valid_short_and_missing_files)
{
    GError *error = NULL;
    gchar *directory = g_dir_make_tmp("test_cr_verifier_key_XXXXXX", &error);
    cr_assert_not_null(directory, "Could not create temporary directory");
    gchar *valid_path = g_build_filename(directory, "valid.key", NULL);
    gchar *short_path = g_build_filename(directory, "short.key", NULL);
    const guchar expected_key[KEY_SIZE] = {
        0x10, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
        0x90, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };
    g_assert_true(g_file_set_contents(valid_path, (const gchar *)expected_key, KEY_SIZE, &error));
    g_assert_no_error(error);
    const guchar short_key[] = {0x01, 0x02, 0x03};
    g_assert_true(g_file_set_contents(short_path, (const gchar *)short_key, sizeof(short_key), &error));
    g_assert_no_error(error);

    guchar loaded_key[KEY_SIZE] = {0};
    cr_assert_eq(cr_readMasterKey(valid_path, loaded_key), TRUE);
    cr_assert_arr_eq(loaded_key, expected_key, KEY_SIZE);
    cr_assert_eq(cr_readMasterKey(short_path, loaded_key), FALSE);
    cr_assert_eq(cr_readMasterKey(NULL, loaded_key), FALSE);

    gchar *missing_path = g_build_filename(directory, "missing.key", NULL);
    cr_assert_eq(cr_readMasterKey(missing_path, loaded_key), FALSE);
    g_unlink(valid_path);
    g_unlink(short_path);
    g_rmdir(directory);
    g_free(missing_path);
    g_free(valid_path);
    g_free(short_path);
    g_free(directory);
}

Test(cr_verifier_helpers, decrypt_log_checks_cmac_before_decrypting)
{
    cr_KEY_TYPE key = {
        0x10, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
        0x90, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };
    guchar iv[IV_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
    };
    const gchar *message = "<134>host app: verifier helper";
    guchar padded[MESSAGE_LEN_SLOGCR] = {0};
    guchar ciphertext[MESSAGE_LEN_SLOGCR] = {0};
    guchar encrypted[CIPHERTEXT_LEN] = {0};
    guchar mac[MAC_LEN] = {0};
    gsize mac_size = 0;

    memcpy(padded, message, strlen(message));
    cr_assert_eq(cr_AES_256_CTR_encrypt(padded, MESSAGE_LEN_SLOGCR, key, iv, ciphertext),
                 MESSAGE_LEN_SLOGCR);
    memcpy(encrypted, iv, IV_SIZE);
    memcpy(encrypted + IV_SIZE, ciphertext, MESSAGE_LEN_SLOGCR);
    cr_assert_eq(cr_CMAC(key, ciphertext, MESSAGE_LEN_SLOGCR, mac, &mac_size, sizeof(mac)), 1);
    cr_assert_eq(mac_size, MAC_LEN);
    memcpy(encrypted + IV_SIZE + MESSAGE_LEN_SLOGCR, mac, MAC_LEN);

    cr_XOR_TYPE aligned_encrypted = {0};
    memcpy(aligned_encrypted, encrypted, sizeof(encrypted));
    GString *decrypted = cr_decryptLog(key, aligned_encrypted);
    cr_assert_not_null(decrypted);
    cr_assert_str_eq(decrypted->str, message);
    g_string_free(decrypted, TRUE);

    aligned_encrypted[IV_SIZE + MESSAGE_LEN_SLOGCR] ^= 0x01;
    decrypted = cr_decryptLog(key, aligned_encrypted);
    cr_assert_not_null(decrypted);
    cr_assert_eq(decrypted->len, 0, "Invalid CMAC must produce no plaintext");
    g_string_free(decrypted, TRUE);
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
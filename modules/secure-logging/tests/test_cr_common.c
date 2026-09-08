/*
 * Unit Tests for Secure Logging / Crash Recovery Module
 * Framework: Criterion
 */

#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <glib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "cr_pi_shared.h"
#include "cr_crypto.h"
#include "cr_randomstuff.h"
#include "utils_slog.h"

/* =========================================================================
 * TEST SUITE: Crypto Core & PRG
 * ========================================================================= */

Test(crypto_core, prg128_context_and_generation)
{
    guchar seed[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    guchar buffer[32] = {0};

    cr_PRG128Context *ctx = cr_CreatePRG128Context(seed);
    cr_assert_not_null(ctx, "PRG128 Context creation failed.");
    cr_assert_eq(ctx->counter, 0, "Initial counter must be 0.");
    cr_assert_arr_eq(ctx->seed, seed, 16, "Seed was not copied correctly.");

    int res = cr_PRG128(ctx, buffer, sizeof(buffer));
    cr_assert_eq(res, 1, "cr_PRG128 execution failed.");
    cr_assert_neq(ctx->counter, 0, "Counter should have incremented after PRG run.");

    /* Verify buffer is no longer all zeroes */
    guchar zero_buf[32] = {0};
    cr_assert_arr_neq(buffer, zero_buf, 32, "PRG output buffer remained zeroed.");

    g_free(ctx);
}

Test(crypto_core, aes256_ctr_encrypt_decrypt)
{
    guchar key[KEY_SIZE] = "01234567890123456789012345678901";
    guchar iv[IV_SIZE]   = "1234567890123456";
    const char *plaintext = "Syslog-ng Secure Logging Crash Recovery Test Payload";
    gint plain_len = (gint)strlen(plaintext) + 1;

    guchar ciphertext[256] = {0};
    guchar decrypted[256]  = {0};

    /* Encrypt */
    int enc_res = cr_AES_256_CTR_encrypt((guchar *)plaintext, plain_len, key, iv, ciphertext);
    cr_assert_eq(enc_res, plain_len, "AES-256-CTR Encryption failed.");
    cr_assert_arr_neq(ciphertext, (guchar *)plaintext, plain_len, "Ciphertext matches plaintext!");

    /* Decrypt */
    int dec_len = cr_AES_256_CTR_decrypt(ciphertext, plain_len, key, iv, decrypted);
    cr_assert_eq(dec_len, plain_len, "AES-256-CTR Decryption returned invalid length.");
    cr_assert_str_eq((char *)decrypted, plaintext, "Decrypted text does not match original plaintext.");
}

Test(crypto_core, cmac_and_prf)
{
    guchar key[KEY_SIZE] = "SecretMasterKeyForSecureLogging!";
    guchar input[] = "LogMessagePayload";
    gsize input_size = sizeof(input);
    guchar output[16] = {0};
    gsize output_len = 0;

    /* Test CMAC */
    int cmac_res = cr_CMAC(key, input, input_size, output, &output_len, sizeof(output));
    cr_assert_eq(cmac_res, 1, "cr_CMAC failed.");
    cr_assert_gt(output_len, 0, "CMAC output length must be > 0.");

    /* Test PRF */
    guchar prf_output[32] = {0};
    int prf_res = cr_PRF(input, input_size, key, prf_output, sizeof(prf_output));
    cr_assert_eq(prf_res, 1, "cr_PRF failed.");
}

Test(crypto_core, subkey_derivation)
{
    guchar master[KEY_SIZE] = "MasterSessionKey32BytesLong12345";
    guchar encKey[KEY_SIZE] = {0};
    guchar drnKey[KEY_SIZE] = {0};
    guchar tagKey[KEY_SIZE] = {0};
    guchar idKey[KEY_SIZE]  = {0};

    int res = cr_DeriveSubKeys(master, encKey, drnKey, tagKey, idKey);
    cr_assert_eq(res, 1, "cr_DeriveSubKeys failed.");

    /* Ensure derived keys are not empty and mutually distinct */
    guchar zero[KEY_SIZE] = {0};
    cr_assert_arr_neq(encKey, zero, KEY_SIZE);
    cr_assert_arr_neq(drnKey, encKey, KEY_SIZE);
    cr_assert_arr_neq(tagKey, drnKey, KEY_SIZE);
    cr_assert_arr_neq(idKey, tagKey, KEY_SIZE);
}

/* =========================================================================
 * TEST SUITE: Random Number Generators
 * ========================================================================= */

Test(random_gen, uniform_random_int_bounds)
{
    guchar seed[KEY_SIZE] = "RandomSeedKey32BytesLongValue!!";
    cr_PRGContext *ctx = cr_CreatePRGContext(seed);
    cr_assert_not_null(ctx);

    const guint upper_bound = 10;
    guint val = 0;

    for (int i = 0; i < 100; ++i)
    {
        int res = cr_UniformRandomInt(ctx, upper_bound, &val);
        cr_assert_eq(res, 1, "cr_UniformRandomInt failed.");
        cr_assert_lt(val, upper_bound, "Random int exceeded upper bound!");
    }

    g_free(ctx);
}

Test(random_gen, distinct_random_ez)
{
    gsize range = 50;
    gint k = 10;
    gint seed = 12345;

    gsize *random_array = cr_distinctRandomEz(range, k, seed);
    cr_assert_not_null(random_array, "cr_distinctRandomEz returned NULL.");

    /* Check uniqueness and bounds */
    for (gint i = 0; i < k; ++i)
    {
        cr_assert_leq(random_array[i], range, "Random element out of range.");
        for (gint j = i + 1; j < k; ++j)
        {
            cr_assert_neq(random_array[i], random_array[j], "Duplicate value found in distinct random array.");
        }
    }

    g_free(random_array);
}

/* =========================================================================
 * TEST SUITE: Utilities & String Handling
 * ========================================================================= */

Test(utils, get_stem_manually_parsing)
{
    gchar *stem1 = get_stem_manually("log_2026.txt");
    cr_assert_str_eq(stem1, "log_2026");
    g_free(stem1);

    gchar *stem2 = get_stem_manually("archive.tar.gz");
    cr_assert_str_eq(stem2, "archive.tar");
    g_free(stem2);

    gchar *stem3 = get_stem_manually("no_extension");
    cr_assert_str_eq(stem3, "no_extension");
    g_free(stem3);
}

Test(utils, get_path_from_file_validation)
{
    char dir_buf[256] = {0};

    /* Invalid NULL arguments */
    cr_assert_eq(get_path_from_file(NULL, dir_buf, sizeof(dir_buf)), FALSE);
    cr_assert_eq(get_path_from_file("/tmp/test.txt", NULL, sizeof(dir_buf)), FALSE);

    /* Valid path extraction */
    gboolean res = get_path_from_file("/tmp/test_file.log", dir_buf, sizeof(dir_buf));
    cr_assert_eq(res, TRUE);
    cr_assert_str_eq(dir_buf, "/tmp");
}

/* =========================================================================
 * TEST SUITE: Time Helpers
 * ========================================================================= */

Test(time_helpers, minutes_seconds_conversion)
{
    int minutes = 0;
    int seconds = 0;

    /* 125000 ms = 125 s = 2 minutes, 5 seconds */
    get_minutes_seconds_from_ms(125000, &minutes, &seconds);
    cr_assert_eq(minutes, 2, "Expected 2 minutes.");
    cr_assert_eq(seconds, 5, "Expected 5 seconds.");
}

Test(time_helpers, timespec_diff_calculation)
{
    struct timespec t1 = {.tv_sec = 10, .tv_nsec = 500000000L};
    struct timespec t2 = {.tv_sec = 12, .tv_nsec = 200000000L};
    struct timespec td = {0};

    diff_timespec(t1, t2, &td);

    /* 12.2s - 10.5s = 1.7s = 1 sec, 700000000 nsec */
    cr_assert_eq(td.tv_sec, 1);
    cr_assert_eq(td.tv_nsec, 700000000L);

    int64_t ms = timespec_as_milliseconds(td);
    cr_assert_eq(ms, 1700, "Expected 1700 milliseconds.");
}
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
#include <string.h>
#include "apphook.h"
#include "mainloop.h"
#include "driver.h"
#include "plugin.h"
#include "cr_destination.h"
#include "cr_destination_worker.h"
#include "cr_destination-parser.h"

/* Extern declaration of module_info from cr_destination_plugin.c */
extern const ModuleInfo module_info;
/* Helper fixture to allocate and free a minimal CrDestinationDriver */
static CrDestinationDriver *driver;

TestSuite(cr_destination_config, .init = app_startup, .fini = app_shutdown);
TestSuite(cr_destination_worker, .init = app_startup, .fini = app_shutdown);
TestSuite(cr_destination_cleanup, .init = app_startup, .fini = app_shutdown);

static void setup_driver(void)
{
    main_thread_handle = get_thread_id();
    /* Initialize instance without full syslog-ng GlobalConfig */
    driver = (CrDestinationDriver *)cr_destination_dd_new(NULL);
    cr_assert_not_null(driver, "Failed to instantiate CrDestinationDriver");
}

static void teardown_driver(void)
{
if (driver)
{
/* Free allocated GStrings and Driver memory */
    if (driver->super.super.super.super.free_fn)
    {
        driver->super.super.super.super.free_fn((LogPipe *)driver);
    }
driver = NULL;
}
}
/*
======================================================================
===
* TEST SUITE: Driver Setters & Configuration

*
======================================================================
=== */
Test(cr_destination_config, set_filenametoken, .init = setup_driver, .fini = teardown_driver)
{
    const gchar *token = "/tmp/test_slog_output";
    cr_destination_dd_set_filenametoken((LogDriver *)driver, token);
    cr_assert_not_null(driver->gstr_filenametoken);
    cr_assert_str_eq(driver->gstr_filenametoken->str, token,

    "Filenametoken string does not match configured value");
}

Test(cr_destination_config, set_keypath, .init = setup_driver, .fini = teardown_driver)
{
    const gchar *keypath = "/tmp/test_slog/master.key";
    cr_destination_dd_set_keypath((LogDriver *)driver, keypath);
    cr_assert_not_null(driver->gstr_keypath);
    cr_assert_str_eq(driver->gstr_keypath->str, keypath,
    "Keypath string does not match configured value");
}

Test(cr_destination_config, set_dir, .init = setup_driver, .fini = teardown_driver)
{
    const gchar *working_dir = "/tmp/test_slog/data";
    cr_destination_dd_set_dir((LogDriver *)driver, working_dir);
    cr_assert_not_null(driver->gstr_dir);
    cr_assert_str_eq(driver->gstr_dir->str, working_dir,

    "Directory path string does not match configured value");
}

Test(cr_destination_config, set_logrotcnt, .init = setup_driver, .fini = teardown_driver)
{
    const gsize first_rotation_cnt = THE_K + 1;
    const gsize second_rotation_cnt = 1024;

    cr_destination_dd_set_logrotcnt((LogDriver *)driver, first_rotation_cnt);
    cr_assert_eq(driver->n_logrotcnt, first_rotation_cnt,
                 "n_logrotcnt was not correctly updated in driver");
    cr_assert_eq(driver->loggerctx.maxLogs, first_rotation_cnt,
                 "loggerctx.maxLogs was not updated to match logrotcnt");

    cr_destination_dd_set_logrotcnt((LogDriver *)driver, second_rotation_cnt);
    cr_assert_eq(driver->n_logrotcnt, second_rotation_cnt,
                 "n_logrotcnt was not replaceable after initial configuration");
    cr_assert_eq(driver->loggerctx.maxLogs, second_rotation_cnt,
                 "loggerctx.maxLogs was not updated after replacement");
}

/*
======================================================================
===

* TEST SUITE: Log Mode Parsing
*
======================================================================
=== */
Test(cr_destination_config, set_mode_variants, .init = setup_driver, .fini = teardown_driver)
{
    /* Test Mode: direct */
    cr_destination_dd_set_mode((LogDriver *)driver, "direct");
    cr_assert_eq(driver->log_mode, CR_LOGMODE_DIRECT, "Expected CR_LOGMODE_DIRECT");
    cr_assert_str_eq(driver->gstr_mode->str, "direct");

    cr_destination_dd_set_mode((LogDriver *)driver, "DIRECT");
    cr_assert_eq(driver->log_mode, CR_LOGMODE_DIRECT, "Mode parsing should ignore ASCII case");
    cr_assert_str_eq(driver->gstr_mode->str, "DIRECT");

    /* Test Mode: base64 */
    cr_destination_dd_set_mode((LogDriver *)driver, "base64");
    cr_assert_eq(driver->log_mode, CR_LOGMODE_BASE64, "Expected CR_LOGMODE_BASE64");
    cr_assert_str_eq(driver->gstr_mode->str, "base64");

    /* Test Mode: enc */
    cr_destination_dd_set_mode((LogDriver *)driver, "enc");
    cr_assert_eq(driver->log_mode, CR_LOGMODE_ENC, "Expected CR_LOGMODE_ENC");
    cr_assert_str_eq(driver->gstr_mode->str, "enc");

    /* Test Mode: NULL (Fallback to enc) */
    cr_destination_dd_set_mode((LogDriver *)driver, NULL);
    cr_assert_eq(driver->log_mode, CR_LOGMODE_ENC, "Fallback should be CR_LOGMODE_ENC on NULL");

    /* Test Mode: Unknown mode (Fallback to enc) */
    cr_destination_dd_set_mode((LogDriver *)driver, "invalid_mode");
    cr_assert_eq(driver->log_mode, CR_LOGMODE_ENC, "Fallback should be CR_LOGMODE_ENC on invalid string");
}
/*
======================================================================
===
* TEST SUITE: Worker Lifecycle & Construction
*
======================================================================
=== */
Test(cr_destination_worker, construct_worker, .init = setup_driver, .fini = teardown_driver)
{
    LogThreadedDestWorker *worker = cr_destination_dw_new((LogThreadedDestDriver
    *)driver, 0);

    cr_assert_not_null(worker, "Failed to instantiate CrDestinationWorker");
    cr_assert_not_null(worker->init, "Worker init function pointer is NULL");
    cr_assert_not_null(worker->deinit, "Worker deinit function pointer is NULL");
    cr_assert_not_null(worker->insert, "Worker insert function pointer is NULL");
    cr_assert_not_null(worker->free_fn, "Worker free function pointer is NULL");
    cr_assert_not_null(worker->connect, "Worker connect function pointer is NULL");
    cr_assert_not_null(worker->disconnect, "Worker disconnect function pointer is NULL");
    
    /* Cleanup worker */
    if (worker->free_fn)
    {
        worker->free_fn(worker);
    }
}
/*
======================================================================
===
* TEST SUITE: Context Cleanup Helper
*
======================================================================
=== */
Test(cr_destination_cleanup, free_logger_pi_contexts_safe_handling, .init = setup_driver,
.fini = teardown_driver)
{
    driver->loggerctx.p_OutputDirectoryPath = g_strdup("/tmp/test_slog");
    driver->loggerctx.p_MasterKeyPath = g_strdup("/tmp/test_slog/master.key");
    driver->loggerctx.p_OutputEncLogPath = g_strdup("/tmp/test_part1.enc");
    driver->p_pictx = g_new0(cr_PIContext, 1);
    driver->p_pictx->keyPath = g_strdup("/tmp/test_slog/currentSession.key");
    driver->p_pictx->logFile = tmpfile();
    driver->p_pictx->keyFile = tmpfile();
    cr_assert_not_null(driver->p_pictx->logFile);
    cr_assert_not_null(driver->p_pictx->keyFile);
    driver->p_prg = g_new0(cr_PRGContext, 1);

    cr_destination_dd_free_logger_pi_contexts(driver);
    cr_assert_null(driver->loggerctx.p_OutputDirectoryPath);
    cr_assert_null(driver->loggerctx.p_MasterKeyPath);
    cr_assert_null(driver->loggerctx.p_OutputEncLogPath,
    "p_OutputEncLogPath was not set to NULL after free");
    cr_assert_null(driver->p_pictx,
    "p_pictx was not set to NULL after free");
    cr_assert_null(driver->p_prg, "Owned PRG context was not freed");
}

Test(cr_destination_cleanup, null_context_is_safe)
{
    cr_destination_dd_free_logger_pi_contexts(NULL);
}
/* src/vfs/gcs - tests for gcs_build_gs_url() function.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/vfs/gcs"

#include "tests/mctest.h"

#include "src/vfs/gcs/gcs.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */
/* @DataSource("test_build_gs_url_ds") */
static const struct test_build_gs_url_ds
{
    const char *bucket;
    const char *path;
    const char *expected;
} test_build_gs_url_ds[] = {
    // 0. NULL path
    {
        "my-bucket",
        NULL,
        "gs://my-bucket",
    },
    // 1. empty path
    {
        "my-bucket",
        "",
        "gs://my-bucket",
    },
    // 2. root path "/"
    {
        "my-bucket",
        "/",
        "gs://my-bucket",
    },
    // 3. dot path "."
    {
        "my-bucket",
        ".",
        "gs://my-bucket",
    },
    // 4. dot-slash path "./"
    {
        "my-bucket",
        "./",
        "gs://my-bucket",
    },
    // 5. absolute path
    {
        "my-bucket",
        "/some/path",
        "gs://my-bucket/some/path",
    },
    // 6. relative path
    {
        "my-bucket",
        "some/path",
        "gs://my-bucket/some/path",
    },
    // 7. dot-slash relative path
    {
        "my-bucket",
        "./some/path",
        "gs://my-bucket/some/path",
    },
    // 8. single file
    {
        "test-bucket",
        "file.txt",
        "gs://test-bucket/file.txt",
    },
    // 9. deeply nested path
    {
        "data-bucket",
        "/a/b/c/d/e",
        "gs://data-bucket/a/b/c/d/e",
    },
};

/* @Test(dataSource = "test_build_gs_url_ds") */
START_PARAMETRIZED_TEST (test_build_gs_url, test_build_gs_url_ds)
{
    // given
    char *result;

    // when
    result = gcs_build_gs_url (data->bucket, data->path);

    // then
    mctest_assert_str_eq (result, data->expected);

    g_free (result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_build_gs_url, test_build_gs_url_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */

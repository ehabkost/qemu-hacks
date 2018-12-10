/*
 * Copyright (C) 2016, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include "qht-bench.inc.c"

#define TEST_QHT_STRING "tests/qht-bench 1>/dev/null 2>&1 -R -S0.1 -D10000 -N1 "

static void test_qht(int n, int u, int d)
{
    n_rw_threads = n;
    update_rate = u / 100.0;
    g_assert(update_rate <= 1.0);
    duration = d;

    qht_bench(false);
}

static void test_2th0u1s(void)
{
    test_qht(2, 0, 1);
}

static void test_2th20u1s(void)
{
    test_qht(2, 20, 1);
}

static void test_2th0u5s(void)
{
    test_qht(2, 0, 5);
}

static void test_2th20u5s(void)
{
    test_qht(2, 20, 5);
}

static void set_common_opts(void)
{
    /* options common to all tests */
    qht_mode |= QHT_MODE_AUTO_RESIZE;
    resize_rate = 0.001;
    resize_delay = 10000;
    n_rz_threads = 1;
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    set_common_opts();

    if (g_test_quick()) {
        g_test_add_func("/qht/parallel/2threads-0%updates-1s", test_2th0u1s);
        g_test_add_func("/qht/parallel/2threads-20%updates-1s", test_2th20u1s);
    } else {
        g_test_add_func("/qht/parallel/2threads-0%updates-5s", test_2th0u5s);
        g_test_add_func("/qht/parallel/2threads-20%updates-5s", test_2th20u5s);
    }
    return g_test_run();
}

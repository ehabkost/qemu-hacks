/*
 * qtest stubs
 *
 * Copyright (c) 2014 Linaro Limited
 * Written by Peter Maydell
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "sysemu/qtest.h"

/* Needed for qtest_allowed() */
bool qtest_allowed;

void qtest_init(const char *qtest_chrdev, const char *qtest_log, Error **errp)
{
    error_setg(errp, "qtest support not compiled in");
}

bool qtest_driver(void)
{
    return false;
}

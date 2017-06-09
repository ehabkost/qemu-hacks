/*
 * Unit tests for QAPI utility functions
 *
 * Copyright (C) 2017 Red Hat Inc.
 *
 * Authors:
 *  Markus Armbruster <armbru@redhat.com>,
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/util.h"
#include "test-qapi-types.h"

static void test_qapi_enum_parse(void)
{
    Error *err = NULL;
    int ret;

    ret = qapi_enum_parse(QType_lookup, NULL, QTYPE__MAX, QTYPE_NONE,
                          &error_abort);
    g_assert_cmpint(ret, ==, QTYPE_NONE);

    ret = qapi_enum_parse(QType_lookup, "junk", QTYPE__MAX, -1,
                          IGNORE_ERRORS);
    g_assert_cmpint(ret, ==, -1);

    ret = qapi_enum_parse(QType_lookup, "junk", QTYPE__MAX, -1,
                          &err);
    error_free_or_abort(&err);

    ret = qapi_enum_parse(QType_lookup, "none", QTYPE__MAX, -1,
                          &error_abort);
    g_assert_cmpint(ret, ==, QTYPE_NONE);

    ret = qapi_enum_parse(QType_lookup, QType_lookup[QTYPE__MAX - 1],
                          QTYPE__MAX, QTYPE__MAX - 1,
                          &error_abort);
    g_assert_cmpint(ret, ==, QTYPE__MAX - 1);
}

static void test_parse_qapi_name(void)
{
    int ret;

    /* Must start with a letter */
    ret = parse_qapi_name("a", true);
    g_assert(ret == 1);
    ret = parse_qapi_name("a$", false);
    g_assert(ret == 1);
    ret = parse_qapi_name("", false);
    g_assert(ret == -1);
    ret = parse_qapi_name("1", false);
    g_assert(ret == -1);

    /* Only letters, digits, hyphen, underscore */
    ret = parse_qapi_name("A-Za-z0-9_", true);
    g_assert(ret == 10);
    ret = parse_qapi_name("A-Za-z0-9_$", false);
    g_assert(ret == 10);
    ret = parse_qapi_name("A-Za-z0-9_$", true);
    g_assert(ret == -1);

    /* __RFQDN_ */
    ret = parse_qapi_name("__com.redhat_supports", true);
    g_assert(ret == 21);
    ret = parse_qapi_name("_com.example_", false);
    g_assert(ret == -1);
    ret = parse_qapi_name("__com.example", false);
    g_assert(ret == -1);
    ret = parse_qapi_name("__com.example_", false);
    g_assert(ret == -1);
}

static void successfn(Error **errp)
{
    g_assert(!ERR_IS_SET(errp));
}

static void fail1(Error **errp)
{
    g_assert(!ERR_IS_SET(errp));

    error_setg(errp, "error1");

    g_assert(ERR_IS_SET(errp));
}

static void fail2(Error **errp)
{
    g_assert(!ERR_IS_SET(errp));

    error_setg(errp, "error2");

    g_assert(ERR_IS_SET(errp));
}

static void multifn(Error **errp)
{
    Error *err1 = NULL, *err2 = NULL;
    successfn(&err1);
    g_assert(!err1);

    fail1(&err1);
    g_assert(err1);
    g_assert_cmpstr(error_get_pretty(err1), ==, "error1");

    fail2(&err2);
    g_assert(err2);
    g_assert_cmpstr(error_get_pretty(err2), ==, "error2");

    error_propagate(&err1, err2);
    g_assert(err1);
    g_assert_cmpstr(error_get_pretty(err1), ==, "error1");

    error_propagate(errp, err1);
}

static void test_propagate(void (*fn)(Error **), Error **errp)
{
    bool failed;
    Error *local_err = NULL;

    g_assert(!ERR_IS_SET(errp));

    fn(&local_err);
    failed = !!local_err;
    error_propagate(errp, local_err);

    g_assert((failed == !!ERR_IS_SET(errp)));
}

static void test_error_api(void)
{
    Error *err = NULL;

    successfn(IGNORE_ERRORS);
    test_propagate(successfn, IGNORE_ERRORS);

    fail1(IGNORE_ERRORS);
    test_propagate(fail1, IGNORE_ERRORS);

    multifn(IGNORE_ERRORS);
    test_propagate(multifn, IGNORE_ERRORS);

    successfn(&err);
    g_assert(!err);

    test_propagate(successfn, &err);
    g_assert(!err);

    fail1(&err);
    g_assert(err);
    g_assert_cmpstr(error_get_pretty(err), ==, "error1");
    error_free_or_abort(&err);

    test_propagate(fail1, &err);
    g_assert(err);
    g_assert_cmpstr(error_get_pretty(err), ==, "error1");
    error_free_or_abort(&err);

    multifn(&err);
    g_assert(err);
    g_assert_cmpstr(error_get_pretty(err), ==, "error1");
    error_free_or_abort(&err);

    test_propagate(multifn, &err);
    g_assert(err);
    g_assert_cmpstr(error_get_pretty(err), ==, "error1");
    error_free_or_abort(&err);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/qapi/util/qapi_enum_parse", test_qapi_enum_parse);
    g_test_add_func("/qapi/util/parse_qapi_name", test_parse_qapi_name);
    g_test_add_func("/qapi/util/error", test_error_api);
    g_test_run();
    return 0;
}

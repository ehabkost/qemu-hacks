/* Unit tests for QOM helpers
 *
 * Authors:
 *  Eduardo Habkost <ehabkost@redhat.com>
 *
 * Copyright (c) 2014 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <glib.h>

#include "qemu/module.h"
#include "qom/object.h"
#include "qom/helpers.h"

#define TYPE_OPENABLE "openable-object"
#define OPENABLE(obj) \
    OBJECT_CHECK(Openable, (obj), TYPE_OPENABLE)
#define OPENABLE_GET_CLASS(obj) \
    OBJECT_GET_CLASS(OpenableClass, (obj), TYPE_OPENABLE)
#define OPENABLE_CLASS(klass) \
    OBJECT_CLASS_CHECK(OpenableClass, (klass), TYPE_OPENABLE)

#define TYPE_OPEN_ONLY "open-only-object"
#define TYPE_OPEN_CLOSE "open-close-object"

typedef struct OpenableClass {
    ObjectClass parent_class;
    FlipOpenFunc open;
    FlipCloseFunc close;
} OpenableClass;

typedef struct Openable {
    Object parent_obj;
    bool opened;
    bool open_called;
    bool open_should_fail, close_should_fail;
} Openable;

static void openable_open(Object *obj, Error **errp)
{
    Openable *op = OPENABLE(obj);
    op->open_called = true;
    if (op->open_should_fail) {
        error_setg(errp, "open failed");
        return;
    }
}

static bool close_called;
static void openable_close(Object *obj, Error **errp)
{
    Openable *op = OPENABLE(obj);
    close_called = true;
    if (op->close_should_fail) {
        error_setg(errp, "close failed");
        return;
    }
}

static void openable_initfn(Object *obj)
{
    Openable *op = OPENABLE(obj);
    OpenableClass *opc = OPENABLE_GET_CLASS(obj);
    object_add_flip_property(obj, "opened", &op->opened, opc->open, opc->close,
                             &error_abort);
}

static const TypeInfo openable_type = {
    .name = TYPE_OPENABLE,
    .parent = TYPE_OBJECT,
    .class_size = sizeof(OpenableClass),
    .instance_size = sizeof(Openable),
    .instance_init = openable_initfn,
};

/* Class with no close function */
static void open_only_class_init(ObjectClass *oc, void *data)
{
    OpenableClass *opc = OPENABLE_CLASS(oc);
    opc->open = openable_open;
}

static const TypeInfo open_only_type = {
    .name = TYPE_OPEN_ONLY,
    .parent = TYPE_OPENABLE,
    .class_init = open_only_class_init,
};

/* Class with open and close function */
static void open_close_class_init(ObjectClass *oc, void *data)
{
    OpenableClass *opc = OPENABLE_CLASS(oc);
    opc->open = openable_open;
    opc->close = openable_close;
}

static const TypeInfo open_close_type = {
    .name = TYPE_OPEN_CLOSE,
    .parent = TYPE_OPENABLE,
    .class_init = open_close_class_init,
};

static void open_should_work(Openable *op)
{
    Error *err = NULL;
    op->open_called = false;
    object_property_set_bool(OBJECT(op), true, "opened", &err);
    g_assert(!err);
    g_assert(op->opened);
    g_assert(op->open_called);
}

static void open_should_nop(Openable *op)
{
    Error *err = NULL;
    op->open_called = false;
    object_property_set_bool(OBJECT(op), true, "opened", &err);
    g_assert(!err);
    g_assert(op->opened);
    g_assert(!op->open_called);
}

static void close_should_work(Openable *op)
{
    Error *err = NULL;
    close_called = false;
    object_property_set_bool(OBJECT(op), false, "opened", &err);
    g_assert(!err);
    g_assert(!op->opened);
    g_assert(close_called);
}

static void close_should_nop(Openable *op)
{
    Error *err = NULL;
    close_called = false;
    object_property_set_bool(OBJECT(op), false, "opened", &err);
    g_assert(!err);
    g_assert(!op->opened);
    g_assert(!close_called);
}

static void test_flip_no_open(void)
{
    Openable *op = OPENABLE(object_new(TYPE_OPENABLE));
    g_assert(!op->open_called);
    g_assert(!op->opened);

    /* already closed when created: */
    close_should_nop(op);

    open_should_nop(op);
    open_should_nop(op);

    close_should_nop(op);
    close_should_nop(op);

    object_unref(OBJECT(op));
}

static void test_flip_open(void)
{
    Error *err = NULL;
    Openable *op = OPENABLE(object_new(TYPE_OPEN_ONLY));
    g_assert(!op->open_called);
    g_assert(!op->opened);

    /* already closed when created: */
    close_should_nop(op);

    open_should_work(op);

    /* Shouldn't call the open function again if already opened: */
    open_should_nop(op);

    /* No close function -> can't set opened=false */
    close_called = false;
    object_property_set_bool(OBJECT(op), false, "opened", &err);
    g_assert(err);
    g_assert(op->opened);
    g_assert(!close_called);

    object_unref(OBJECT(op));
}

static void test_flip_open_fail(void)
{
    Error *err = NULL;
    Openable *op = OPENABLE(object_new(TYPE_OPEN_ONLY));
    g_assert(!op->open_called);
    g_assert(!op->opened);

    /* starts already closed: */
    close_should_nop(op);

    op->open_should_fail = true;
    object_property_set_bool(OBJECT(op), true, "opened", &err);
    g_assert(err);
    g_assert(op->open_called);
    g_assert(!op->opened);

    /* still closed: */
    close_should_nop(op);

    /* try again: */
    op->open_called = false;
    object_property_set_bool(OBJECT(op), true, "opened", &err);
    g_assert(err);
    g_assert(op->open_called);
    g_assert(!op->opened);

    /* still closed: */
    close_should_nop(op);

    object_unref(OBJECT(op));
}

static void test_flip_close(void)
{
    Openable *op = OPENABLE(object_new(TYPE_OPEN_CLOSE));
    g_assert(!op->open_called);
    g_assert(!op->opened);

    close_called = false;

    /* Open and close it twice: */
    open_should_work(op);
    open_should_nop(op);
    close_should_work(op);
    close_should_nop(op);

    open_should_work(op);
    open_should_nop(op);
    close_should_work(op);
    close_should_nop(op);

    /* deleting the object while open should call the close function, too: */
    open_should_work(op);

    close_called = false;
    object_unref(OBJECT(op));
    g_assert(close_called);
}


int main(int argc, char *argv[])
{

    g_test_init(&argc, &argv, NULL);
    module_call_init(MODULE_INIT_QOM);
    type_register_static(&openable_type);
    type_register_static(&open_only_type);
    type_register_static(&open_close_type);

    g_test_add_func("/qom/helpers/flip/no_open_func",
                    test_flip_no_open);
    g_test_add_func("/qom/helpers/flip/open",
                    test_flip_open);
    g_test_add_func("/qom/helpers/flip/open_fail",
                    test_flip_open_fail);
    g_test_add_func("/qom/helpers/flip/close",
                    test_flip_close);
    g_test_run();
    return 0;
}

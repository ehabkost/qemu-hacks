/* Utility helpers for QOM object handling
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

#include "qom/object.h"
#include "qom/helpers.h"
#include "qapi/visitor.h"
#include "qapi/qmp/qerror.h"


typedef struct FlipProperty {
    bool *ptr;
    FlipOpenFunc open;
    FlipCloseFunc close;
} FlipProperty;

static void flip_close(Object *obj, FlipProperty *prop, Error **errp)
{
    Error *err = NULL;
    if (!*prop->ptr) {
        return; /* already closed */
    }
    if (prop->open && !prop->close) {
        error_set(errp, QERR_PERMISSION_DENIED);
        return;
    }
    if (prop->close) {
        prop->close(obj, &err);
    }
    if (err) {
        error_propagate(errp, err);
        return;
    }
    *prop->ptr = false;
}

static void flip_open(Object *obj, FlipProperty *prop, Error **errp)
{
    Error *err = NULL;
    if (*prop->ptr) {
        return; /* already open */
    }
    if (prop->open) {
        prop->open(obj, &err);
    }
    if (err) {
        error_propagate(errp, err);
        return;
    }
    *prop->ptr = true;
}

static void flip_get(Object *obj, Visitor *v, void *opaque,
                     const char *name, Error **errp)
{
    FlipProperty *prop = opaque;
    bool value = *prop->ptr;
    visit_type_bool(v, &value, name, errp);
}

static void flip_set(Object *obj, Visitor *v, void *opaque,
                     const char *name, Error **errp)
{
    FlipProperty *prop = opaque;
    Error *err = NULL;
    bool value;

    visit_type_bool(v, &value, name, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    if (value) {
        flip_open(obj, prop, errp);
    } else if (!value) {
        flip_close(obj, prop, errp);
    }
}

static void flip_release(Object *obj, const char *name, void *opaque)
{
    FlipProperty *prop = opaque;
    flip_close(obj, prop, NULL);
    g_free(prop);
}

void object_add_flip_property(Object *obj, const char *name, bool *ptr,
                              FlipOpenFunc open, FlipCloseFunc close,
                              Error **errp)
{
    Error *err = NULL;
    FlipProperty *prop = g_new0(FlipProperty, 1);

    assert(ptr);

    prop->ptr = ptr;
    prop->open = open;
    prop->close = close;
    object_property_add(obj, name, "bool", flip_get, flip_set, flip_release,
                        prop, &err);
    if (err) {
        error_propagate(errp, err);
        g_free(prop);
    }
}

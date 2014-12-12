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
#ifndef QOM_HELPERS_H
#define QOM_HELPERS_H

#include "qapi/error.h"
#include "qemu/typedefs.h"

typedef void (*FlipOpenFunc)(Object *obj, Error **errp);
typedef void (*FlipCloseFunc)(Object *obj, Error **errp);

/**
 * object_add_flip_property:
 * @obj: the object to add a property to
 * @name: the name of the property
 * @ptr: pointer to boolean field to keep open/closed state
 * @open: function to be called when property is changed to true
 * @close: function to be called when property is changed to false
 * @errp: if an error occurs, a pointer to an area to store the area
 *
 * A flip property is a boolean property that will call an open function
 * when it is set to true, and a close function when it is set to false.
 *
 * If the open function is NULL, that means no action is required when flipping
 * the property to true, and the setter will always succeed.
 *
 * If an open function is provided but no close function, that means the
 * property can be set to true but can't be reverted back to false.
 * This can be used for objects that can't be closed/unloaded once they are
 * opened/unloaded.
 *
 * If the property is removed while set to true, the close function will be
 * called automatically.
 */
void object_add_flip_property(Object *obj, const char *name, bool *ptr,
                              FlipOpenFunc open, FlipCloseFunc close,
                              Error **errp);

#endif

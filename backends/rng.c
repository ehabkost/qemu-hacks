/*
 * QEMU Random Number Generator Backend
 *
 * Copyright IBM, Corp. 2012
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "sysemu/rng.h"
#include "qapi/qmp/qerror.h"
#include "qom/object_interfaces.h"
#include "qom/helpers.h"

void rng_backend_request_entropy(RngBackend *s, size_t size,
                                 EntropyReceiveFunc *receive_entropy,
                                 void *opaque)
{
    RngBackendClass *k = RNG_BACKEND_GET_CLASS(s);

    if (k->request_entropy) {
        k->request_entropy(s, size, receive_entropy, opaque);
    }
}

void rng_backend_cancel_requests(RngBackend *s)
{
    RngBackendClass *k = RNG_BACKEND_GET_CLASS(s);

    if (k->cancel_requests) {
        k->cancel_requests(s);
    }
}

static void rng_backend_complete(UserCreatable *uc, Error **errp)
{
    object_property_set_bool(OBJECT(uc), true, "opened", errp);
}

static void rng_backend_init(Object *obj)
{
    RngBackend *s = RNG_BACKEND(obj);
    RngBackendClass *k = RNG_BACKEND_GET_CLASS(s);
    object_add_flip_property(obj, "opened", &s->opened,
                             k->opened,
                             NULL,
                             &error_abort);
}

static void rng_backend_class_init(ObjectClass *oc, void *data)
{
    UserCreatableClass *ucc = USER_CREATABLE_CLASS(oc);

    ucc->complete = rng_backend_complete;
}

static const TypeInfo rng_backend_info = {
    .name = TYPE_RNG_BACKEND,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(RngBackend),
    .instance_init = rng_backend_init,
    .class_size = sizeof(RngBackendClass),
    .class_init = rng_backend_class_init,
    .abstract = true,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_USER_CREATABLE },
        { }
    }
};

static void register_types(void)
{
    type_register_static(&rng_backend_info);
}

type_init(register_types);

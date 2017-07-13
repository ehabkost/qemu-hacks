/*
 *  Dynamic device configuration and creation -- buses.
 *
 *  Copyright (c) 2009 CodeSourcery
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/qdev.h"
#include "qapi/error.h"
#include "qapi/clone-visitor.h"
#include "qapi-visit.h"
#include "qapi/qmp/qstring.h"

static void qbus_set_hotplug_handler_internal(BusState *bus, Object *handler,
                                              Error **errp)
{

    object_property_set_link(OBJECT(bus), OBJECT(handler),
                             QDEV_HOTPLUG_HANDLER_PROPERTY, errp);
}

void qbus_set_hotplug_handler(BusState *bus, DeviceState *handler, Error **errp)
{
    qbus_set_hotplug_handler_internal(bus, OBJECT(handler), errp);
}

void qbus_set_bus_hotplug_handler(BusState *bus, Error **errp)
{
    qbus_set_hotplug_handler_internal(bus, OBJECT(bus), errp);
}

int qbus_walk_children(BusState *bus,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque)
{
    BusChild *kid;
    int err;

    if (pre_busfn) {
        err = pre_busfn(bus, opaque);
        if (err) {
            return err;
        }
    }

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        err = qdev_walk_children(kid->child,
                                 pre_devfn, pre_busfn,
                                 post_devfn, post_busfn, opaque);
        if (err < 0) {
            return err;
        }
    }

    if (post_busfn) {
        err = post_busfn(bus, opaque);
        if (err) {
            return err;
        }
    }

    return 0;
}

static void qbus_realize(BusState *bus, DeviceState *parent, const char *name)
{
    const char *typename = object_get_typename(OBJECT(bus));
    BusClass *bc;
    int i, bus_id;

    bus->parent = parent;

    if (name) {
        bus->name = g_strdup(name);
    } else if (bus->parent && bus->parent->id) {
        /* parent device has id -> use it plus parent-bus-id for bus name */
        bus_id = bus->parent->num_child_bus;
        bus->name = g_strdup_printf("%s.%d", bus->parent->id, bus_id);
    } else {
        /* no id -> use lowercase bus type plus global bus-id for bus name */
        bc = BUS_GET_CLASS(bus);
        bus_id = bc->automatic_ids++;
        bus->name = g_strdup_printf("%s.%d", typename, bus_id);
        for (i = 0; bus->name[i]; i++) {
            bus->name[i] = qemu_tolower(bus->name[i]);
        }
    }

    if (bus->parent) {
        QLIST_INSERT_HEAD(&bus->parent->child_bus, bus, sibling);
        bus->parent->num_child_bus++;
        object_property_add_child(OBJECT(bus->parent), bus->name, OBJECT(bus), NULL);
        object_unref(OBJECT(bus));
    } else if (bus != sysbus_get_default()) {
        /* TODO: once all bus devices are qdevified,
           only reset handler for main_system_bus should be registered here. */
        qemu_register_reset(qbus_reset_all_fn, bus);
    }
}

static void bus_unparent(Object *obj)
{
    BusState *bus = BUS(obj);
    BusChild *kid;

    while ((kid = QTAILQ_FIRST(&bus->children)) != NULL) {
        DeviceState *dev = kid->child;
        object_unparent(OBJECT(dev));
    }
    if (bus->parent) {
        QLIST_REMOVE(bus, sibling);
        bus->parent->num_child_bus--;
        bus->parent = NULL;
    } else {
        assert(bus != sysbus_get_default()); /* main_system_bus is never freed */
        qemu_unregister_reset(qbus_reset_all_fn, bus);
    }
}

void qbus_create_inplace(void *bus, size_t size, const char *typename,
                         DeviceState *parent, const char *name)
{
    object_initialize(bus, size, typename);
    qbus_realize(bus, parent, name);
}

BusState *qbus_create(const char *typename, DeviceState *parent, const char *name)
{
    BusState *bus;

    bus = BUS(object_new(typename));
    qbus_realize(bus, parent, name);

    return bus;
}

static bool bus_get_realized(Object *obj, Error **errp)
{
    BusState *bus = BUS(obj);

    return bus->realized;
}

static void bus_set_realized(Object *obj, bool value, Error **errp)
{
    BusState *bus = BUS(obj);
    BusClass *bc = BUS_GET_CLASS(bus);
    BusChild *kid;
    Error *local_err = NULL;

    if (value && !bus->realized) {
        if (bc->realize) {
            bc->realize(bus, &local_err);
        }

        /* TODO: recursive realization */
    } else if (!value && bus->realized) {
        QTAILQ_FOREACH(kid, &bus->children, sibling) {
            DeviceState *dev = kid->child;
            object_property_set_bool(OBJECT(dev), false, "realized",
                                     &local_err);
            if (local_err != NULL) {
                break;
            }
        }
        if (bc->unrealize && local_err == NULL) {
            bc->unrealize(bus, &local_err);
        }
    }

    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

    bus->realized = value;
}

static void qbus_initfn(Object *obj)
{
    BusState *bus = BUS(obj);
    BusClass *bc = BUS_GET_CLASS(bus);

    QTAILQ_INIT(&bus->children);

    if (bc->device_type) {
        bus->accepted_device_types = g_new0(strList, 1);
        bus->accepted_device_types->value = g_strdup(bc->device_type);
    }

    object_property_add_link(obj, QDEV_HOTPLUG_HANDLER_PROPERTY,
                             TYPE_HOTPLUG_HANDLER,
                             (Object **)&bus->hotplug_handler,
                             object_property_allow_set_link,
                             0,
                             NULL);
    object_property_add_bool(obj, "realized",
                             bus_get_realized, bus_set_realized, NULL);
}

static char *default_bus_get_fw_dev_path(DeviceState *dev)
{
    return g_strdup(object_get_typename(OBJECT(dev)));
}

static void bus_get_device_type(Object *obj, Visitor *v,
                                 const char *name, void *opaque,
                                 Error **errp)
{
    BusState *bus = BUS(obj);

    visit_type_strList(v, NULL, &bus->accepted_device_types, errp);
}

bool qbus_is_full(BusState *bus)
{
    BusClass *bus_class = BUS_GET_CLASS(bus);
    return bus_class->max_dev && bus->max_index >= bus_class->max_dev;
}

/* Generic slot enumeration function that will return a generic-slot slot type.
 */
static DeviceSlotInfoList *bus_generic_enumerate_slots(BusState *bus, Error **errp)
{
    Error *local_err = NULL;
    DeviceSlotInfoList *r = g_new0(DeviceSlotInfoList, 1);

    r->value = g_new0(DeviceSlotInfo, 1);
    r->value->accepted_device_types = QAPI_CLONE(strList, bus->accepted_device_types);

    r->value->hotpluggable = qbus_is_hotpluggable(bus);

    r->value->incomplete = true;

    /* Conditions that make a bus unavailable:
     * - Bus already full
     * - Hotplug when the bus is not hotpluggable
     */
    r->value->available =
        !(qbus_is_full(bus) ||
          (qdev_hotplug && !qbus_is_hotpluggable(bus)));


    /* r->value->props = { 'bus': bus->name } */
    r->value->props = g_new0(SlotOptionList, 1);
    r->value->props->value = g_new0(SlotOption, 1);
    r->value->props->value->option = g_strdup("bus");
    r->value->props->value->values = g_new0(ValueSet, 1);
    r->value->props->value->values->type = VALUE_SET_KIND_LIST;;
    r->value->props->value->values->u.list.data = g_new0(anyList, 1);
    r->value->props->value->values->u.list.data->value = QOBJECT(qstring_from_str(bus->name));

    error_propagate(errp, local_err);
    return r;
}

static void bus_class_init(ObjectClass *class, void *data)
{
    BusClass *bc = BUS_CLASS(class);

    class->unparent = bus_unparent;
    bc->get_fw_dev_path = default_bus_get_fw_dev_path;
    bc->enumerate_slots = bus_generic_enumerate_slots;

    object_class_property_add(class, "accepted-device-types", "strList",
                              bus_get_device_type, NULL, NULL, NULL,
                              &error_abort);
}

static void qbus_finalize(Object *obj)
{
    BusState *bus = BUS(obj);

    g_free(bus->name);
}

static const TypeInfo bus_info = {
    .name = TYPE_BUS,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(BusState),
    .abstract = true,
    .class_size = sizeof(BusClass),
    .instance_init = qbus_initfn,
    .instance_finalize = qbus_finalize,
    .class_init = bus_class_init,
};

static void bus_register_types(void)
{
    type_register_static(&bus_info);
}

type_init(bus_register_types)

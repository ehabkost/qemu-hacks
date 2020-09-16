/*
 *  Copyright (c) 2013-2018 Laurent Vivier <laurent@vivier.eu>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/nubus/mac-nubus-bridge.h"


static void mac_nubus_bridge_init(Object *obj)
{
    MacNubusState *s = MAC_NUBUS_BRIDGE(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    s->bus = NUBUS_BUS(qbus_create(TYPE_NUBUS_BUS, DEVICE(s), NULL));

    sysbus_init_mmio(sbd, &s->bus->super_slot_io);
    sysbus_init_mmio(sbd, &s->bus->slot_io);
}

static void mac_nubus_bridge_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "Nubus bridge";
}

OBJECT_DEFINE_TYPE_EXTENDED(mac_nubus_bridge_info,
                            MacNubusState, void,
                            MAC_NUBUS_BRIDGE, NUBUS_BRIDGE,
    .instance_init = mac_nubus_bridge_init,
    .class_init    = mac_nubus_bridge_class_init,
)



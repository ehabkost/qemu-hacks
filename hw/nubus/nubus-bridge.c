/*
 * QEMU Macintosh Nubus
 *
 * Copyright (c) 2013-2018 Laurent Vivier <laurent@vivier.eu>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/nubus/nubus.h"

static void nubus_bridge_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->fw_name = "nubus";
}

OBJECT_DEFINE_TYPE_EXTENDED(nubus_bridge_info,
                            void, void,
                            NUBUS_BRIDGE, SYS_BUS_DEVICE,
    .class_init    = nubus_bridge_class_init,
)



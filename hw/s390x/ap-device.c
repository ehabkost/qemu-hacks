/*
 * Adjunct Processor (AP) matrix device
 *
 * Copyright 2018 IBM Corp.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/s390x/ap-device.h"

static void ap_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "AP device class";
    dc->hotpluggable = false;
}

OBJECT_DEFINE_TYPE_EXTENDED(ap_device_info,
                            APDevice, DeviceClass,
                            AP_DEVICE, DEVICE,
    .class_init = ap_class_init,
    .abstract = true,
)



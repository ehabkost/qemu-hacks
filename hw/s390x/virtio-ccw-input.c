/*
 * virtio ccw input implementation
 *
 * Copyright 2012, 2015 IBM Corp.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "virtio-ccw.h"

static void virtio_ccw_input_realize(VirtioCcwDevice *ccw_dev, Error **errp)
{
    VirtIOInputCcw *dev = VIRTIO_INPUT_CCW(ccw_dev);
    DeviceState *vdev = DEVICE(&dev->vdev);

    qdev_realize(vdev, BUS(&ccw_dev->bus), errp);
}

static Property virtio_ccw_input_properties[] = {
    DEFINE_PROP_BIT("ioeventfd", VirtioCcwDevice, flags,
                    VIRTIO_CCW_FLAG_USE_IOEVENTFD_BIT, true),
    DEFINE_PROP_UINT32("max_revision", VirtioCcwDevice, max_rev,
                       VIRTIO_CCW_MAX_REV),
    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_ccw_input_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtIOCCWDeviceClass *k = VIRTIO_CCW_DEVICE_CLASS(klass);

    k->realize = virtio_ccw_input_realize;
    device_class_set_props(dc, virtio_ccw_input_properties);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
}

static void virtio_ccw_keyboard_instance_init(Object *obj)
{
    VirtIOInputHIDCcw *dev = VIRTIO_INPUT_HID_CCW(obj);
    VirtioCcwDevice *ccw_dev = VIRTIO_CCW_DEVICE(obj);

    ccw_dev->force_revision_1 = true;
    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_KEYBOARD);
}

static void virtio_ccw_mouse_instance_init(Object *obj)
{
    VirtIOInputHIDCcw *dev = VIRTIO_INPUT_HID_CCW(obj);
    VirtioCcwDevice *ccw_dev = VIRTIO_CCW_DEVICE(obj);

    ccw_dev->force_revision_1 = true;
    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_MOUSE);
}

static void virtio_ccw_tablet_instance_init(Object *obj)
{
    VirtIOInputHIDCcw *dev = VIRTIO_INPUT_HID_CCW(obj);
    VirtioCcwDevice *ccw_dev = VIRTIO_CCW_DEVICE(obj);

    ccw_dev->force_revision_1 = true;
    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_TABLET);
}

OBJECT_DEFINE_TYPE_EXTENDED(virtio_ccw_input,
                            VirtIOInputCcw, void,
                            VIRTIO_INPUT_CCW, VIRTIO_CCW_DEVICE,
    .class_init    = virtio_ccw_input_class_init,
    .abstract = true,
)

OBJECT_DEFINE_TYPE_EXTENDED(virtio_ccw_input_hid,
                            VirtIOInputHIDCcw, void,
                            VIRTIO_INPUT_HID_CCW, VIRTIO_INPUT_CCW,
    .abstract = true,
)

OBJECT_DEFINE_TYPE_EXTENDED(virtio_ccw_keyboard,
                            void, void,
                            VIRTIO_KEYBOARD_CCW, VIRTIO_INPUT_HID_CCW,
    .instance_init = virtio_ccw_keyboard_instance_init,
)

OBJECT_DEFINE_TYPE_EXTENDED(virtio_ccw_mouse,
                            void, void,
                            VIRTIO_MOUSE_CCW, VIRTIO_INPUT_HID_CCW,
    .instance_init = virtio_ccw_mouse_instance_init,
)

OBJECT_DEFINE_TYPE_EXTENDED(virtio_ccw_tablet,
                            void, void,
                            VIRTIO_TABLET_CCW, VIRTIO_INPUT_HID_CCW,
    .instance_init = virtio_ccw_tablet_instance_init,
)



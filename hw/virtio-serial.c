/*
 * Virtio Console and Generic Serial Port Devices
 *
 * Copyright Red Hat, Inc. 2009
 *
 * Authors:
 *  Amit Shah <amit.shah@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu-char.h"
#include "virtio-serial.h"

typedef struct VirtConsole {
    VirtIOSerialPort port;
    CharDriverState *chr;
} VirtConsole;


/* Callback function that's called when the guest sends us data */
static ssize_t flush_buf(VirtIOSerialPort *port, const uint8_t *buf, size_t len)
{
    VirtConsole *vcon = DO_UPCAST(VirtConsole, port, port);

    return qemu_chr_write(vcon->chr, buf, len);
}

/* Readiness of the guest to accept data on a port */
static int chr_can_read(void *opaque)
{
    VirtConsole *vcon = opaque;

    return virtio_serial_guest_ready(&vcon->port);
}

/* Send data from a char device over to the guest */
static void chr_read(void *opaque, const uint8_t *buf, int size)
{
    VirtConsole *vcon = opaque;

    virtio_serial_write(&vcon->port, buf, size);
}

static void chr_event(void *opaque, int event)
{
    VirtConsole *vcon = opaque;

    switch (event) {
    case CHR_EVENT_OPENED: {
        virtio_serial_open(&vcon->port);
        break;
    }
    case CHR_EVENT_CLOSED:
        virtio_serial_close(&vcon->port);
        break;
    }
}

/* Virtio Console Ports */
static int virtconsole_initfn(VirtIOSerialDevice *dev)
{
    VirtIOSerialPort *port = DO_UPCAST(VirtIOSerialPort, dev, &dev->qdev);
    VirtConsole *vcon = DO_UPCAST(VirtConsole, port, port);

    port->info = dev->info;

    port->is_console = true;

    /*
     * For console ports, just assume the guest is ready to accept our
     * data.
     */
    port->guest_connected = true;

    if (vcon->chr) {
        qemu_chr_add_handlers(vcon->chr, chr_can_read, chr_read, chr_event,
                              vcon);
    }
    return 0;
}

static int virtconsole_exitfn(VirtIOSerialDevice *dev)
{
    VirtIOSerialPort *port = DO_UPCAST(VirtIOSerialPort, dev, &dev->qdev);
    VirtConsole *vcon = DO_UPCAST(VirtConsole, port, port);

    if (vcon->chr) {
        qemu_chr_close(vcon->chr);
    }

    return 0;
}

static VirtIOSerialPortInfo virtconsole_info = {
    .qdev.name     = "virtconsole",
    .qdev.size     = sizeof(VirtConsole),
    .init          = virtconsole_initfn,
    .exit          = virtconsole_exitfn,
    .have_data     = flush_buf,
    .qdev.props = (Property[]) {
        DEFINE_PROP_UINT8("is_console", VirtConsole, port.is_console, 1),
        DEFINE_PROP_CHR("chardev", VirtConsole, chr),
        DEFINE_PROP_END_OF_LIST(),
    },
};

static void virtconsole_register(void)
{
    virtio_serial_port_qdev_register(&virtconsole_info);
}
device_init(virtconsole_register)

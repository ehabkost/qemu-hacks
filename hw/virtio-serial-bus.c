/*
 * A bus for connecting virtio serial and console ports
 *
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * Author(s):
 *  Amit Shah <amit.shah@redhat.com>
 *
 * Some earlier parts are:
 *  Copyright IBM, Corp. 2008
 * authored by
 *  Christian Ehrhardt <ehrhardt@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "monitor.h"
#include "qemu-queue.h"
#include "sysbus.h"
#include "virtio-serial.h"

/* The virtio-serial bus on top of which the ports will ride as devices */
struct VirtIOSerialBus {
    BusState qbus;

    /* This is the parent device that provides the bus for ports. */
    VirtIOSerial *vser;

    /* The maximum number of ports that can ride on top of this bus */
    uint32_t max_nr_ports;
};

struct VirtIOSerial {
    VirtIODevice vdev;

    VirtQueue *c_ivq, *c_ovq;
    /* Arrays of ivqs and ovqs: one per port */
    VirtQueue **ivqs, **ovqs;

    VirtIOSerialBus *bus;

    QTAILQ_HEAD(, VirtIOSerialPort) ports;
    struct virtio_console_config config;
};

/* This struct holds individual buffers received for each port */
typedef struct VirtIOSerialPortBuffer {
    QTAILQ_ENTRY(VirtIOSerialPortBuffer) next;

    uint8_t *buf;

    size_t len; /* length of the buffer */
    size_t offset; /* offset from which to consume data in the buffer */

    bool previous_failure; /* Did sending out this buffer fail previously? */
} VirtIOSerialPortBuffer;

static VirtIOSerialPort *find_port_by_id(VirtIOSerial *vser, uint32_t id)
{
    VirtIOSerialPort *port;

    QTAILQ_FOREACH(port, &vser->ports, next) {
        if (port->id == id)
            return port;
    }
    return NULL;
}

static VirtIOSerialPort *find_port_by_vq(VirtIOSerial *vser, VirtQueue *vq)
{
    VirtIOSerialPort *port;

    QTAILQ_FOREACH(port, &vser->ports, next) {
        if (port->ivq == vq || port->ovq == vq)
            return port;
    }
    return NULL;
}

static bool use_multiport(VirtIOSerial *vser)
{
    return vser->vdev.features & (1 << VIRTIO_CONSOLE_F_MULTIPORT);
}

static size_t write_to_port(VirtIOSerialPort *port,
                            const uint8_t *buf, size_t size)
{
    VirtQueueElement elem;
    VirtQueue *vq;
    size_t offset = 0;
    size_t len = 0;

    vq = port->ivq;
    if (!virtio_queue_ready(vq)) {
        return 0;
    }
    if (!size) {
        return 0;
    }

    while (offset < size) {
        int i;

        if (!virtqueue_pop(vq, &elem)) {
            break;
        }

        for (i = 0; offset < size && i < elem.in_num; i++) {
            len = MIN(elem.in_sg[i].iov_len, size - offset);

            memcpy(elem.in_sg[i].iov_base, buf + offset, len);
            offset += len;
        }
        virtqueue_push(vq, &elem, len);
    }

    virtio_notify(&port->vser->vdev, vq);
    return offset;
}

static size_t send_control_msg(VirtIOSerialPort *port, void *buf, size_t len)
{
    VirtQueueElement elem;
    VirtQueue *vq;
    struct virtio_console_control *cpkt;

    vq = port->vser->c_ivq;
    if (!virtio_queue_ready(vq)) {
        return 0;
    }
    if (!virtqueue_pop(vq, &elem)) {
        return 0;
    }

    cpkt = (struct virtio_console_control *)buf;
    stl_p(&cpkt->id, port->id);
    memcpy(elem.in_sg[0].iov_base, buf, len);

    virtqueue_push(vq, &elem, len);
    virtio_notify(&port->vser->vdev, vq);
    return len;
}

static size_t send_control_event(VirtIOSerialPort *port, uint16_t event,
                                 uint16_t value)
{
    struct virtio_console_control cpkt;

    stw_p(&cpkt.event, event);
    stw_p(&cpkt.value, value);

    return send_control_msg(port, &cpkt, sizeof(cpkt));
}

static void init_buf(VirtIOSerialPortBuffer *buf, uint8_t *buffer, size_t len)
{
    buf->buf = buffer;
    buf->len = len;
    buf->offset = 0;
    buf->previous_failure = false;
}

static VirtIOSerialPortBuffer *alloc_buf(size_t len)
{
    VirtIOSerialPortBuffer *buf;

    buf = qemu_malloc(sizeof(*buf));
    buf->buf = qemu_malloc(len);

    init_buf(buf, buf->buf, len);

    return buf;
}

static void free_buf(VirtIOSerialPortBuffer *buf)
{
    qemu_free(buf->buf);
    qemu_free(buf);
}

static void flush_queue(VirtIOSerialPort *port)
{
    VirtIOSerialPortBuffer *buf;
    size_t out_size;
    ssize_t ret;

    while ((buf = QTAILQ_FIRST(&port->unflushed_buffers))) {
        QTAILQ_REMOVE(&port->unflushed_buffers, buf, next);

        out_size = buf->len - buf->offset;
        if (!port->host_connected) {
            port->nr_bytes -= buf->len + buf->offset;
            free_buf(buf);
            continue;
        }

        ret = port->info->have_data(port, buf->buf + buf->offset, out_size);
        if (ret < out_size) {
            QTAILQ_INSERT_HEAD(&port->unflushed_buffers, buf, next);
        }
        if (ret <= 0) {
            /* We're not progressing at all */
            if (buf->previous_failure) {
                break;
            }
            buf->previous_failure = true;
        } else {
            buf->offset += ret;
            port->nr_bytes -= ret;
            buf->previous_failure = false;
        }
        if (!(buf->len - buf->offset)) {
            free_buf(buf);
        }
    }

    if (port->host_throttled && port->nr_bytes < port->byte_limit) {
        port->host_throttled = false;
        send_control_event(port, VIRTIO_CONSOLE_THROTTLE_PORT, 0);
    }
}

static void flush_all_ports(VirtIOSerial *vser)
{
    struct VirtIOSerialPort *port;

    QTAILQ_FOREACH(port, &vser->ports, next) {
        if (port->has_activity) {
            port->has_activity = false;
            flush_queue(port);
        }
    }
}

static void remove_port_buffers(VirtIOSerialPort *port)
{
    struct VirtIOSerialPortBuffer *buf, *buf2;

    QTAILQ_FOREACH_SAFE(buf, &port->unflushed_buffers, next, buf2) {
        QTAILQ_REMOVE(&port->unflushed_buffers, buf, next);
        free_buf(buf);
    }
}

/* Functions for use inside qemu to open and read from/write to ports */
int virtio_serial_open(VirtIOSerialPort *port)
{
    /* Don't allow opening an already-open port */
    if (port->host_connected) {
        return 0;
    }
    /* Send port open notification to the guest */
    port->host_connected = true;
    send_control_event(port, VIRTIO_CONSOLE_PORT_OPEN, 1);

    return 0;
}

int virtio_serial_close(VirtIOSerialPort *port)
{
    port->host_connected = false;
    send_control_event(port, VIRTIO_CONSOLE_PORT_OPEN, 0);

    remove_port_buffers(port);
    return 0;
}

/* Individual ports/apps call this function to write to the guest. */
ssize_t virtio_serial_write(VirtIOSerialPort *port, const uint8_t *buf,
                            size_t size)
{
    if (!port || !port->host_connected || !port->guest_connected) {
        return 0;
    }
    return write_to_port(port, buf, size);
}

/*
 * Readiness of the guest to accept data on a port.
 * Returns max. data the guest can receive
 */
size_t virtio_serial_guest_ready(VirtIOSerialPort *port)
{
    VirtQueue *vq = port->ivq;

    if (!virtio_queue_ready(vq) ||
        !(port->vser->vdev.status & VIRTIO_CONFIG_S_DRIVER_OK) ||
        virtio_queue_empty(vq)) {
        return 0;
    }
    if (use_multiport(port->vser) && !port->guest_connected) {
        return 0;
    }

    if (virtqueue_avail_bytes(vq, 4096, 0)) {
        return 4096;
    }
    if (virtqueue_avail_bytes(vq, 1, 0)) {
        return 1;
    }
    return 0;
}

/* Guest wants to notify us of some event */
static void handle_control_message(VirtIOSerial *vser, void *buf)
{
    struct VirtIOSerialPort *port;
    struct virtio_console_control cpkt, *gcpkt;
    uint8_t *buffer;
    size_t buffer_len;

    gcpkt = buf;
    port = find_port_by_id(vser, ldl_p(&gcpkt->id));
    if (!port)
        return;

    cpkt.event = lduw_p(&gcpkt->event);
    cpkt.value = lduw_p(&gcpkt->value);

    switch(cpkt.event) {
    case VIRTIO_CONSOLE_PORT_READY:
        /*
         * Now that we know the guest asked for the port name, we're
         * sure the guest has initialised whatever state is necessary
         * for this port. Now's a good time to let the guest know if
         * this port is a console port so that the guest can hook it
         * up to hvc.
         */
        if (port->is_console) {
            send_control_event(port, VIRTIO_CONSOLE_CONSOLE_PORT, 1);
        }
        if (port->name) {
            stw_p(&cpkt.event, VIRTIO_CONSOLE_PORT_NAME);
            stw_p(&cpkt.value, 1);

            buffer_len = sizeof(cpkt) + strlen(port->name) + 1;
            buffer = qemu_malloc(buffer_len);

            memcpy(buffer, &cpkt, sizeof(cpkt));
            memcpy(buffer + sizeof(cpkt), port->name, strlen(port->name));
            buffer[buffer_len - 1] = 0;

            send_control_msg(port, buffer, buffer_len);
            qemu_free(buffer);
        }

        if (port->host_connected) {
            send_control_event(port, VIRTIO_CONSOLE_PORT_OPEN, 1);
        }
        /*
         * When the guest has asked us for this information it means
         * the guest is all setup and has its virtqueues
         * initialised. If some app is interested in knowing about
         * this event, let it know.
         */
        if (port->info->guest_ready) {
            port->info->guest_ready(port);
        }
        break;
    case VIRTIO_CONSOLE_PORT_OPEN:
        port->guest_connected = cpkt.value;
        if (cpkt.value && port->info->guest_open) {
            /* Send the guest opened notification if an app is interested */
            port->info->guest_open(port);
        }
        if (!cpkt.value && port->info->guest_close) {
            /* Send the guest closed notification if an app is interested */
            port->info->guest_close(port);
        }
        break;
    }
}

static void control_in(VirtIODevice *vdev, VirtQueue *vq)
{
}

static void control_out(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtQueueElement elem;
    VirtIOSerial *vser;

    vser = DO_UPCAST(VirtIOSerial, vdev, vdev);

    while (virtqueue_pop(vq, &elem)) {
        handle_control_message(vser, elem.out_sg[0].iov_base);
        virtqueue_push(vq, &elem, elem.out_sg[0].iov_len);
    }
    virtio_notify(vdev, vq);
}

/*
 * Guest wrote something to some port.
 *
 * Flush the data in the entire chunk that we received rather than
 * splitting it into multiple buffers. VNC clients don't consume split
 * buffers
 */
static void handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtIOSerial *vser;
    VirtQueueElement elem;

    vser = DO_UPCAST(VirtIOSerial, vdev, vdev);

    while (virtqueue_pop(vq, &elem)) {
        VirtIOSerialPort *port;
        VirtIOSerialPortBuffer *buf;

        port = find_port_by_vq(vser, vq);
        if (!port) {
            goto next_buf;
        }
        /*
         * A port may not have any handler registered for consuming the
         * data that the guest sends or it may not have a chardev associated
         * with it. Just ignore the data in that case.
         */
        if (!port->info->have_data) {
            goto next_buf;
        }

        /* The guest always sends only one sg */
        buf = alloc_buf(elem.out_sg[0].iov_len);
        memcpy(buf->buf, elem.out_sg[0].iov_base, buf->len);

        QTAILQ_INSERT_TAIL(&port->unflushed_buffers, buf, next);
        port->nr_bytes += buf->len;
        port->has_activity = true;

        if (!port->host_throttled && port->byte_limit &&
            port->nr_bytes >= port->byte_limit) {

            port->host_throttled = true;
            send_control_event(port, VIRTIO_CONSOLE_THROTTLE_PORT, 1);
        }
    next_buf:
        virtqueue_push(vq, &elem, elem.out_sg[0].iov_len);
    }
    virtio_notify(vdev, vq);
    flush_all_ports(vser);
}

static void handle_input(VirtIODevice *vdev, VirtQueue *vq)
{
}

static uint32_t get_features(VirtIODevice *vdev)
{
    return 1 << VIRTIO_CONSOLE_F_MULTIPORT;
}

/* Guest requested config info */
static void get_config(VirtIODevice *vdev, uint8_t *config_data)
{
    VirtIOSerial *vser;

    vser = DO_UPCAST(VirtIOSerial, vdev, vdev);
    memcpy(config_data, &vser->config, sizeof(struct virtio_console_config));
}

static void set_config(VirtIODevice *vdev, const uint8_t *config_data)
{
    struct virtio_console_config config;

    memcpy(&config, config_data, sizeof(config));
}

static void virtio_serial_save(QEMUFile *f, void *opaque)
{
    VirtIOSerial *s = opaque;
    VirtIOSerialPort *port;
    uint32_t nr_active_ports;
    unsigned int nr_bufs;

    /* The virtio device */
    virtio_save(&s->vdev, f);

    /* The config space */
    qemu_put_be16s(f, &s->config.cols);
    qemu_put_be16s(f, &s->config.rows);
    qemu_put_be32s(f, &s->config.nr_ports);

    /* Items in struct VirtIOSerial */

    /* Do this because we might have hot-unplugged some ports */
    nr_active_ports = 0;
    QTAILQ_FOREACH(port, &s->ports, next)
        nr_active_ports++;

    qemu_put_be32s(f, &nr_active_ports);

    /*
     * Items in struct VirtIOSerialPort.
     */
    QTAILQ_FOREACH(port, &s->ports, next) {
        VirtIOSerialPortBuffer *buf;

        /*
         * We put the port number because we may not have an active
         * port at id 0 that's reserved for a console port, or in case
         * of ports that might have gotten unplugged
         */
        qemu_put_be32s(f, &port->id);
        qemu_put_be64s(f, &port->byte_limit);
        qemu_put_be64s(f, &port->nr_bytes);
        qemu_put_byte(f, port->guest_connected);
        qemu_put_byte(f, port->host_throttled);

        /* All the pending buffers from active ports */
        nr_bufs = 0;
        QTAILQ_FOREACH(buf, &port->unflushed_buffers, next) {
            nr_bufs++;
        }
        qemu_put_be32s(f, &nr_bufs);
        if (!nr_bufs) {
            continue;
        }
        QTAILQ_FOREACH(buf, &port->unflushed_buffers, next) {
            qemu_put_be64s(f, &buf->len);
            qemu_put_be64s(f, &buf->offset);
            qemu_put_buffer(f, buf->buf, buf->len);
        }
    }
}

static int virtio_serial_load(QEMUFile *f, void *opaque, int version_id)
{
    VirtIOSerial *s = opaque;
    VirtIOSerialPort *port;
    uint32_t nr_active_ports;
    unsigned int i;

    if (version_id > 2) {
        return -EINVAL;
    }

    /* The virtio device */
    virtio_load(&s->vdev, f);

    if (version_id < 2) {
        return 0;
    }

    /* The config space */
    qemu_get_be16s(f, &s->config.cols);
    qemu_get_be16s(f, &s->config.rows);
    s->config.nr_ports = qemu_get_be32(f);

    /* Items in struct VirtIOSerial */

    qemu_get_be32s(f, &nr_active_ports);

    /* Items in struct VirtIOSerialPort */
    for (i = 0; i < nr_active_ports; i++) {
        VirtIOSerialPortBuffer *buf;
        uint32_t id;
        unsigned int nr_bufs;

        id = qemu_get_be32(f);
        port = find_port_by_id(s, id);

        port->byte_limit = qemu_get_be64(f);
        port->nr_bytes   = qemu_get_be64(f);
        port->guest_connected = qemu_get_byte(f);
        port->host_throttled = qemu_get_byte(f);

        /* All the pending buffers from active ports */
        qemu_get_be32s(f, &nr_bufs);
        if (!nr_bufs) {
            continue;
        }
        for (; nr_bufs; nr_bufs--) {
            size_t len;

            qemu_get_be64s(f, &len);
            buf = alloc_buf(len);

            qemu_get_be64s(f, &buf->offset);
            qemu_get_buffer(f, buf->buf, buf->len);
            QTAILQ_INSERT_TAIL(&port->unflushed_buffers, buf, next);
        }
    }

    return 0;
}

static void virtser_bus_dev_print(Monitor *mon, DeviceState *qdev, int indent);

static struct BusInfo virtser_bus_info = {
    .name      = "virtio-serial-bus",
    .size      = sizeof(VirtIOSerialBus),
    .print_dev = virtser_bus_dev_print,
};

static VirtIOSerialBus *virtser_bus_new(DeviceState *dev)
{
    VirtIOSerialBus *bus;

    bus = FROM_QBUS(VirtIOSerialBus, qbus_create(&virtser_bus_info, dev,
                                                 "virtio-serial-bus"));
    bus->qbus.allow_hotplug = 1;

    return bus;
}

static void virtser_bus_dev_print(Monitor *mon, DeviceState *qdev, int indent)
{
    VirtIOSerialDevice *dev = DO_UPCAST(VirtIOSerialDevice, qdev, qdev);
    VirtIOSerialPort *port = DO_UPCAST(VirtIOSerialPort, dev, &dev->qdev);

    monitor_printf(mon, "%*s dev-prop-int: id: %u\n",
                   indent, "", port->id);
    monitor_printf(mon, "%*s dev-prop-int: guest_connected: %d\n",
                   indent, "", port->guest_connected);
    monitor_printf(mon, "%*s dev-prop-int: host_connected: %d\n",
                   indent, "", port->host_connected);
    monitor_printf(mon, "%*s dev-prop-int: host_throttled: %d\n",
                   indent, "", port->host_throttled);
    monitor_printf(mon, "%*s dev-prop-int: nr_bytes: %zu\n",
                   indent, "", port->nr_bytes);
}

static int virtser_port_qdev_init(DeviceState *qdev, DeviceInfo *base)
{
    VirtIOSerialDevice *dev = DO_UPCAST(VirtIOSerialDevice, qdev, qdev);
    VirtIOSerialPortInfo *info = DO_UPCAST(VirtIOSerialPortInfo, qdev, base);
    VirtIOSerialPort *port = DO_UPCAST(VirtIOSerialPort, dev, &dev->qdev);
    VirtIOSerialBus *bus = DO_UPCAST(VirtIOSerialBus, qbus, qdev->parent_bus);
    int ret;
    bool plugging_port0;

    port->vser = bus->vser;

    /*
     * Is the first console port we're seeing? If so, put it up at
     * location 0. This is done for backward compatibility (old
     * kernel, new qemu).
     */
    plugging_port0 = port->is_console && !find_port_by_id(port->vser, 0);

    if (port->vser->config.nr_ports == bus->max_nr_ports && !plugging_port0) {
        qemu_error("virtio-serial-bus: Maximum device limit reached\n");
        return -1;
    }
    dev->info = info;

    ret = info->init(dev);
    if (ret) {
        return ret;
    }
    QTAILQ_INIT(&port->unflushed_buffers);

    port->id = plugging_port0 ? 0 : port->vser->config.nr_ports++;

    if (!use_multiport(port->vser)) {
        /*
         * Allow writes to guest in this case; we have no way of
         * knowing if a guest port is connected.
         */
        port->guest_connected = true;
    }

    QTAILQ_INSERT_TAIL(&port->vser->ports, port, next);
    port->ivq = port->vser->ivqs[port->id];
    port->ovq = port->vser->ovqs[port->id];

    /* Send an update to the guest about this new port added */
    virtio_notify_config(&port->vser->vdev);

    return ret;
}

static int virtser_port_qdev_exit(DeviceState *qdev)
{
    VirtIOSerialDevice *dev = DO_UPCAST(VirtIOSerialDevice, qdev, qdev);
    VirtIOSerialPort *port = DO_UPCAST(VirtIOSerialPort, dev, &dev->qdev);
    VirtIOSerial *vser = port->vser;

    /*
     * Don't decrement nr_ports here; thus we keep a linearly
     * increasing port id. Not utilising an id again saves us a couple
     * of complications:
     *
     * - Not having to bother about sending the port id to the guest
     *   kernel on hotplug or on addition of new ports; the guest can
     *   also linearly increment the port number. This is preferable
     *   because the config space won't have the need to store a
     *   ports_map.
     *
     * - Extra state to be stored for all the "holes" that got created
     *   so that we keep filling in the ids from the least available
     *   index.
     *
     * When such a functionality is desired, a control message to add
     * a port can be introduced.
     */
    QTAILQ_REMOVE(&vser->ports, port, next);

    if (port->info->exit)
        port->info->exit(dev);

    remove_port_buffers(port);

    return 0;
}

void virtio_serial_port_qdev_register(VirtIOSerialPortInfo *info)
{
    info->qdev.init = virtser_port_qdev_init;
    info->qdev.bus_info = &virtser_bus_info;
    info->qdev.exit = virtser_port_qdev_exit;
    info->qdev.unplug = qdev_simple_unplug_cb;
    qdev_register(&info->qdev);
}

VirtIODevice *virtio_serial_init(DeviceState *dev, uint32_t max_nr_ports)
{
    VirtIOSerial *vser;
    VirtIODevice *vdev;
    uint32_t i;

    if (!max_nr_ports)
        return NULL;

    vdev = virtio_common_init("virtio-serial", VIRTIO_ID_CONSOLE,
                              sizeof(struct virtio_console_config),
                              sizeof(VirtIOSerial));

    vser = DO_UPCAST(VirtIOSerial, vdev, vdev);

    /* Spawn a new virtio-serial bus on which the ports will ride as devices */
    vser->bus = virtser_bus_new(dev);
    vser->bus->vser = vser;
    QTAILQ_INIT(&vser->ports);

    vser->bus->max_nr_ports = max_nr_ports;
    vser->ivqs = qemu_malloc(max_nr_ports * sizeof(VirtQueue *));
    vser->ovqs = qemu_malloc(max_nr_ports * sizeof(VirtQueue *));

    /* Add a queue for host to guest transfers for port 0 (backward compat) */
    vser->ivqs[0] = virtio_add_queue(vdev, 128, handle_input);
    /* Add a queue for guest to host transfers for port 0 (backward compat) */
    vser->ovqs[0] = virtio_add_queue(vdev, 128, handle_output);

    /* control queue: host to guest */
    vser->c_ivq = virtio_add_queue(vdev, 16, control_in);
    /* control queue: guest to host */
    vser->c_ovq = virtio_add_queue(vdev, 16, control_out);

    for (i = 1; i < vser->bus->max_nr_ports; i++) {
        /* Add a per-port queue for host to guest transfers */
        vser->ivqs[i] = virtio_add_queue(vdev, 128, handle_input);
        /* Add a per-per queue for guest to host transfers */
        vser->ovqs[i] = virtio_add_queue(vdev, 128, handle_output);
    }

    vser->config.max_nr_ports = max_nr_ports;
    /*
     * Reserve location 0 for a console port for backward compat
     * (old kernel, new qemu)
     */
    vser->config.nr_ports = 1;

    vser->vdev.get_features = get_features;
    vser->vdev.get_config = get_config;
    vser->vdev.set_config = set_config;

    /*
     * Register for the savevm section with the virtio-console name
     * to preserve backward compat
     */
    register_savevm("virtio-console", -1, 2, virtio_serial_save,
                    virtio_serial_load, vser);

    return vdev;
}

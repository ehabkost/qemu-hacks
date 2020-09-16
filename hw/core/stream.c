#include "qemu/osdep.h"
#include "hw/stream.h"
#include "qemu/module.h"

size_t
stream_push(StreamSlave *sink, uint8_t *buf, size_t len, bool eop)
{
    StreamSlaveClass *k =  STREAM_SLAVE_GET_CLASS(sink);

    return k->push(sink, buf, len, eop);
}

bool
stream_can_push(StreamSlave *sink, StreamCanPushNotifyFn notify,
                void *notify_opaque)
{
    StreamSlaveClass *k =  STREAM_SLAVE_GET_CLASS(sink);

    return k->can_push ? k->can_push(sink, notify, notify_opaque) : true;
}

OBJECT_DEFINE_TYPE_EXTENDED(stream_slave_info,
                            void, StreamSlaveClass,
                            STREAM_SLAVE, INTERFACE)




/*
 * QEMU buffered QEMUFile
 *
 * Copyright IBM, Corp. 2008
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "qemu-common.h"
#include "qemu-file.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "buffered_file.h"

//#define DEBUG_BUFFERED_FILE

typedef struct QEMUFileBuffered
{
    BufferedPutFunc *put_buffer;
    BufferedPutReadyFunc *put_ready;
    BufferedWaitForUnfreezeFunc *wait_for_unfreeze;
    BufferedCloseFunc *close;
    void *opaque;
    QEMUFile *file;
    int freeze_output;
    size_t bytes_xfer;
    size_t xfer_limit;
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    QEMUTimer *timer;
} QEMUFileBuffered;

#ifdef DEBUG_BUFFERED_FILE
#define DPRINTF(fmt, ...) \
    do { printf("buffered-file: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

static void buffered_append(QEMUFileBuffered *s,
                            const uint8_t *buf, size_t size)
{
    if (size > (s->buffer_capacity - s->buffer_size)) {
        void *tmp;

        DPRINTF("increasing buffer capacity from %zu by %zu\n",
                s->buffer_capacity, size + 1024);

        s->buffer_capacity += size + 1024;

        tmp = g_realloc(s->buffer, s->buffer_capacity);
        if (tmp == NULL) {
            fprintf(stderr, "qemu file buffer expansion failed\n");
            exit(1);
        }

        s->buffer = tmp;
    }

    memcpy(s->buffer + s->buffer_size, buf, size);
    s->buffer_size += size;
}


/** Try to write write some buffer contents to the lower-level file
 *
 * Freezes output if the lower-level put_buffer function returns -EAGAIN.
 *
 * Note that in case of errors qemu_file_set_error() is used instead of
 * returning an error code, because the caller may need to know how many
 * bytes were written before the error anyway.
 */
static size_t buffered_try_put_down(QEMUFileBuffered *s, const uint8_t *buffer, size_t size)
{
    size_t offset = 0;
    int error;
    ssize_t ret;

    error = qemu_file_get_error(s->file);
    if (error != 0) {
        DPRINTF("putting buffer down when error, bailing: %s\n", strerror(-error));
        return 0;
    }

    DPRINTF("putting down %zu byte(s) of data\n", size);

    while (!s->freeze_output && offset < size) {
        if (s->bytes_xfer > s->xfer_limit) {
            DPRINTF("transfer limit exceeded when putting\n");
            break;
        }

        ret = s->put_buffer(s->opaque, buffer + offset,
                            size - offset);
        if (ret == -EAGAIN) {
            DPRINTF("backend not ready, freezing\n");
            s->freeze_output = 1;
            break;
        }

        if (ret <= 0) {
            DPRINTF("error putting data down, %zd\n", ret);
            qemu_file_set_error(s->file, ret);
            break;
        }

        DPRINTF("put down %zd byte(s)\n", ret);
        offset += ret;
        s->bytes_xfer += ret;
    }

    DPRINTF("put down %zu of %zu byte(s)\n", offset, size);

    return offset;
}

/** Flush buffer if possible, without waiting for unfreeze
 *
 * There are no guarantees that everything will be flushed.
 */
static void buffered_try_flush(QEMUFileBuffered *s)
{
    size_t offset;

    offset = buffered_try_put_down(s, s->buffer, s->buffer_size);

    if (offset > 0) {
        memmove(s->buffer, s->buffer + offset, s->buffer_size - offset);
        s->buffer_size -= offset;
    }
}

/** Calls put_ready notification function if we're still under the xfer limit
 */
static void buffered_notify_up_if_ready(QEMUFileBuffered *s)
{
    if (s->bytes_xfer <= s->xfer_limit) {
        DPRINTF("notifying client\n");
        s->put_ready(s->opaque);
    }
}

static int buffered_put_buffer(void *opaque, const uint8_t *buf, int64_t pos, int size)
{
    QEMUFileBuffered *s = opaque;
    ssize_t offset, ret;
    int error;

    DPRINTF("putting %d bytes at %" PRId64 "\n", size, pos);

    error = qemu_file_get_error(s->file);
    if (error) {
        DPRINTF("flush when error, bailing: %s\n", strerror(-error));
        return error;
    }

    /* Try to flush what we already have in the buffer, first: */
    DPRINTF("unfreezing output\n");
    s->freeze_output = 0;
    buffered_try_flush(s);

    /* Also, try to write directly from the buffer without copying
     * if possible:
     */
    offset = buffered_try_put_down(s, buf, size);

    /* In case we got errors above, return it because callers can't handle
     * partial writes (yet).
     */
    error = qemu_file_get_error(s->file);
    if (error) {
        ret = error;
    } else {
        /* everything is OK, so append the remaining data to the buffer */
        DPRINTF("buffering %d bytes\n", size - offset);
        buffered_append(s, buf + offset, size - offset);
        ret = size;
    }

    /* Special meaning for put_buffer(NULL, 0, 0): try to flush
     * current buffers and notify client if we're ready.
     */
    if (pos == 0 && size == 0) {
        DPRINTF("file is ready\n");
        buffered_notify_up_if_ready(s);
    }

    return ret;
}

/** Flush everything, waiting for unfreeze if needed
 *
 * Returns 0 on success, or qemu_file_get_error(s->file) if an error happened.
 */
static int buffered_flush(QEMUFileBuffered *s)
{
    while (!qemu_file_get_error(s->file) && s->buffer_size) {
        s->freeze_output = 0;
        buffered_try_flush(s);
        if (s->freeze_output)
            s->wait_for_unfreeze(s->opaque);
    }
    return qemu_file_get_error(s->file);
}

static int buffered_close(void *opaque)
{
    QEMUFileBuffered *s = opaque;
    int ret;

    DPRINTF("closing\n");

    /* we can ignore buffered_flush() errors because it will happen only
     * if s->file has an error set, and qemu_fclose() will return it.
     */
    buffered_flush(s);

    ret = s->close(s->opaque);

    qemu_del_timer(s->timer);
    qemu_free_timer(s->timer);
    g_free(s->buffer);
    g_free(s);

    return ret;
}

/*
 * The meaning of the return values is:
 *   0: We can continue sending
 *   1: Time to stop
 *   negative: There has been an error
 */
static int buffered_rate_limit(void *opaque)
{
    QEMUFileBuffered *s = opaque;
    int ret;

    ret = qemu_file_get_error(s->file);
    if (ret) {
        return ret;
    }
    if (s->freeze_output)
        return 1;

    if (s->bytes_xfer > s->xfer_limit)
        return 1;

    return 0;
}

static int64_t buffered_set_rate_limit(void *opaque, int64_t new_rate)
{
    QEMUFileBuffered *s = opaque;
    if (qemu_file_get_error(s->file)) {
        goto out;
    }
    if (new_rate > SIZE_MAX) {
        new_rate = SIZE_MAX;
    }

    s->xfer_limit = new_rate / 10;
    
out:
    return s->xfer_limit;
}

static int64_t buffered_get_rate_limit(void *opaque)
{
    QEMUFileBuffered *s = opaque;
  
    return s->xfer_limit;
}

static void buffered_rate_tick(void *opaque)
{
    QEMUFileBuffered *s = opaque;

    if (qemu_file_get_error(s->file)) {
        buffered_close(s);
        return;
    }

    qemu_mod_timer(s->timer, qemu_get_clock_ms(rt_clock) + 100);

    if (s->freeze_output)
        return;

    s->bytes_xfer = 0;

    buffered_try_flush(s);

    buffered_notify_up_if_ready(s);
}

QEMUFile *qemu_fopen_ops_buffered(void *opaque,
                                  size_t bytes_per_sec,
                                  BufferedPutFunc *put_buffer,
                                  BufferedPutReadyFunc *put_ready,
                                  BufferedWaitForUnfreezeFunc *wait_for_unfreeze,
                                  BufferedCloseFunc *close)
{
    QEMUFileBuffered *s;

    s = g_malloc0(sizeof(*s));

    s->opaque = opaque;
    s->xfer_limit = bytes_per_sec / 10;
    s->put_buffer = put_buffer;
    s->put_ready = put_ready;
    s->wait_for_unfreeze = wait_for_unfreeze;
    s->close = close;

    /* the "frontend" QEMUFile interface, exposed to the outside
     */
    s->file = qemu_fopen_ops(s, buffered_put_buffer, NULL,
                             buffered_close, buffered_rate_limit,
                             buffered_set_rate_limit,
			     buffered_get_rate_limit);

    s->timer = qemu_new_timer_ms(rt_clock, buffered_rate_tick, s);

    qemu_mod_timer(s->timer, qemu_get_clock_ms(rt_clock) + 100);

    return s->file;
}

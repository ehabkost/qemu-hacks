/* Some basic definitions related to stdio.h or other I/O interfaces
 */
#ifndef QEMU_STDIO_H
#define QEMU_STDIO_H

#include "compiler.h"
#include "config-host.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#ifndef ENOMEDIUM
#define ENOMEDIUM ENODEV
#endif
#if !defined(ENOTSUP)
#define ENOTSUP 4096
#endif
#if !defined(ECANCELED)
#define ECANCELED 4097
#endif
#ifndef TIME_MAX
#define TIME_MAX LONG_MAX
#endif

#ifndef CONFIG_IOVEC
#define CONFIG_IOVEC
struct iovec {
    void *iov_base;
    size_t iov_len;
};
/*
 * Use the same value as Linux for now.
 */
#define IOV_MAX     1024
#else
#include <sys/uio.h>
#endif

typedef int (*fprintf_function)(FILE *f, const char *fmt, ...)
    GCC_FMT_ATTR(2, 3);

#ifdef _WIN32
#define fsync _commit
#if !defined(lseek)
# define lseek _lseeki64
#endif
int qemu_ftruncate64(int, int64_t);
#if !defined(ftruncate)
# define ftruncate qemu_ftruncate64
#endif

static inline char *realpath(const char *path, char *resolved_path)
{
    _fullpath(resolved_path, path, _MAX_PATH);
    return resolved_path;
}
#endif

int qemu_open(const char *name, int flags, ...);
int qemu_close(int fd);

#endif /* QEMU_STDIO_H */

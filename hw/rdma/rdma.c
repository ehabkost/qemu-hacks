/*
 * RDMA device interface
 *
 * Copyright (C) 2018 Oracle
 * Copyright (C) 2018 Red Hat Inc
 *
 * Authors:
 *     Yuval Shaia <yuval.shaia@oracle.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "hw/rdma/rdma.h"
#include "qemu/module.h"

OBJECT_DEFINE_TYPE_EXTENDED(rdma_hmp_info,
                            void, RdmaProviderClass,
                            RDMA_PROVIDER, INTERFACE)



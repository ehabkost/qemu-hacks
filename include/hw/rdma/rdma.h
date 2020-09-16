/*
 * RDMA device interface
 *
 * Copyright (C) 2019 Oracle
 * Copyright (C) 2019 Red Hat Inc
 *
 * Authors:
 *     Yuval Shaia <yuval.shaia@oracle.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef RDMA_H
#define RDMA_H

#include "qom/object.h"

#define TYPE_RDMA_PROVIDER "rdma"

OBJECT_DECLARE_INTERFACE(RdmaProvider, RdmaProviderClass, RDMA_PROVIDER)


struct RdmaProviderClass {
    InterfaceClass parent;

    void (*print_statistics)(Monitor *mon, RdmaProvider *obj);
};

#endif

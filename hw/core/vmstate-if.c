/*
 * VMState interface
 *
 * Copyright (c) 2009-2019 Red Hat Inc
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/vmstate-if.h"

OBJECT_DEFINE_TYPE_EXTENDED(vmstate_if_info,
                            void, VMStateIfClass,
                            VMSTATE_IF, INTERFACE)



/*
 * PCMCIA emulation
 *
 * Copyright 2013 SUSE LINUX Products GmbH
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/pcmcia.h"

OBJECT_DEFINE_TYPE_EXTENDED(pcmcia_card_type_info,
                            PCMCIACardState, PCMCIACardClass,
                            PCMCIA_CARD, DEVICE,
    .abstract = true,
)



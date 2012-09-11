/* Declarations for global properties handling */

#ifndef GLOBAL_PROPS_H
#define GLOBAL_PROPS_H

#include "qemu-queue.h"
#include "hw/qdev.h"

typedef struct GlobalProperty {
    const char *driver;
    const char *property;
    const char *value;
    QTAILQ_ENTRY(GlobalProperty) next;
} GlobalProperty;

void qemu_globals_register_list(GlobalProperty *props);
void qdev_prop_set_globals(DeviceState *dev);
void qemu_add_globals(void);

#endif /* GLOBAL_PROPS_H */

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

/* Get the string value of a global property directly
 *
 * Using this function is discouraged. Code should be converted to use
 * QOM and/or qdev instead of using it. It is provided only as a convenience
 * for code that is not converted yet.
 */
const char *qemu_global_get(const char *driver, const char *property);
bool qemu_global_get_bool(const char *driver, const char *prop, Error **errp);

#endif /* GLOBAL_PROPS_H */

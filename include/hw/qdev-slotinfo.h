#ifndef QDEV_SLOTINFO_H
#define QDEV_SLOTINFO_H

#include "qapi/qmp/qobject.h"
#include "qapi/qmp/qstring.h"

/**
 * valuelist_contains:
 *
 * Returns true if the value list represented by @values
 * contains @v.
 *
 * @values follows the format documented at SlotOption.values
 * in the QAPI schema.
 */
bool valuelist_contains(QObject *values, QObject *v);

/**
 * valuelist_extend:
 *
 * Extend a value list with elements from another value list.
 *
 * Ownership of 'new' is transfered to the function.
 */
void valuelist_extend(QObject **valuelist, QObject *new);

/**
 * slot_options_can_be_combined:
 *
 * Check if two SlotOptionLists can be combined in one.
 *
 * Two slot option lists can be combined if all options have exactly
 * the same value except (at most) one.
 *
 * Returns true if the option lists can be combined.
 *
 * If return value is true, *@opt_name is set to the only
 * mismatching option name.  If all options match, *@opt_name is
 * set to NULL.
 */
bool slot_options_can_be_combined(SlotOptionList *a, SlotOptionList *b,
                                  const char **opt_name);

/*TODO: doc */
bool slots_can_be_combined(DeviceSlotInfo *a, DeviceSlotInfo *b,
	                       const char **opt_name);

/*TODO: doc */
void slots_combine(DeviceSlotInfo *a, DeviceSlotInfo *b, const char *opt_name);

/*TODO: doc */
bool slots_try_combine(DeviceSlotInfo *a, DeviceSlotInfo *b);

/*TODO: doc */
void slot_list_add_slot(DeviceSlotInfoList **l, DeviceSlotInfo *slot);

/*TODO: doc */
void slot_add_prop(DeviceSlotInfo *slot, const char *option, QObject *values);

#define slot_add_prop_str(slot, option, s) \
    slot_add_prop(slot, option, QOBJECT(qstring_from_str(s)));

#define slot_add_prop_int(slot, option, int) \
    slot_add_prop(slot, option, QOBJECT(qnum_from_int(i)));


/*TODO: doc */
DeviceSlotInfo *make_slot(BusState *bus);

#endif /* QDEV_SLOTINFO_H */

#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/qdev-slotinfo.h"
#include "qapi/qmp/qlist.h"
#include "qapi/qmp/qstring.h"
#include "qapi/qmp/qnum.h"
#include "qapi/qmp/qbool.h"
#include "qapi/util.h"

/* Ensure a value list is normalized to a list of values
 *
 * This does NOT normalize individual elements of the list to be
 * in the [A] or [A, B] format.
 *
 * Returns a new reference to the normalized value.
 */
static QList *valuelist_normalize(QObject *values)
{
    if (qobject_type(values) == QTYPE_QLIST) {
        qobject_incref(values);
        return qobject_to_qlist(values);
    } else {
        QList *l = qlist_new();

        qobject_incref(values);
        qlist_append_obj(l, values);
        return l;
    }
}

/* Simplify value list, if possible
 *
 * Onwership of @values is transfered to the function, and a
 * new object is returned.
 */
static QObject *valuelist_simplify(QList *values)
{
    if (qlist_size(values) == 1) {
        QObject *o = qlist_entry_obj(qlist_first(values));
        QType t = qobject_type(o);
        if (t == QTYPE_QNULL ||
            t == QTYPE_QNUM ||
            t == QTYPE_QSTRING ||
            t == QTYPE_QBOOL) {
            qobject_incref(o);
            QDECREF(values);
            return o;
        }
    }

    return QOBJECT(values);
}

/* Check if a given entry of a value list contains @v */
static bool valuelist_entry_contains(QObject *ev, QObject *v)
{
    switch (qobject_type(ev)) {
        case QTYPE_QLIST:
            {
                QList *l = qobject_to_qlist(ev);
                size_t sz = qlist_size(l);
                QObject *a;
                QObject *b;
                QType ta, tb, tv;

                if (sz == 1) {
                    return qobject_compare(qlist_entry_obj(qlist_first(l)), v) == 0;
                }
                if (sz != 2) {
                    return false;
                }

                a = qlist_entry_obj(qlist_first(l));
                b = qlist_entry_obj(qlist_next(qlist_first(l)));
                ta = qobject_type(a);
                tb = qobject_type(b);
                tv = qobject_type(v);

                return ((tv == QTYPE_QNUM || tv == QTYPE_QSTRING) &&
                        (ta == tv) && (tb == tv) &&
                        qobject_compare(a, v) <= 0 &&
                        qobject_compare(v, b) <= 0);
            }
            break;
        case QTYPE_QNUM:
        case QTYPE_QSTRING:
        case QTYPE_QBOOL:
            return qobject_compare(ev, v) == 0;
        default:
            return false;
    }
}

bool valuelist_contains(QObject *values, QObject *v)
{
    QList *l = valuelist_normalize(values);
    QListEntry *e;
    bool r = false;

    QLIST_FOREACH_ENTRY(l, e) {
        QObject *ev = qlist_entry_obj(e);
        if (valuelist_entry_contains(ev, v)) {
            r = true;
            break;
        }
    }

    QDECREF(l);
    return r;
}

void valuelist_extend(QObject **valuelist, QObject *new)
{
    QObject *old = *valuelist;
    QList *l = valuelist_normalize(old);
    QList *newl = valuelist_normalize(new);
    QListEntry *e;

    QLIST_FOREACH_ENTRY(newl, e) {
        QObject *ev = qlist_entry_obj(e);
        qobject_incref(ev);
        /*TODO: combine ranges */
        qlist_append_obj(l, ev);
    }
    QDECREF(newl);

    *valuelist = valuelist_simplify(l);
    qobject_decref(old);
}

static SlotOption *find_slot_option(SlotOptionList *opts, const char *option)
{
    for (; opts; opts = opts->next) {
        if (!strcmp(opts->value->option, option)) {
            return opts->value;
        }
    }
    return NULL;
}

bool slot_options_can_be_combined(SlotOptionList *a, SlotOptionList *b,
                                  const char **opt_name)
{
    SlotOptionList *ol;
    const char *mismatch = NULL;

    /* Check if all options in @b will be handled when we loop through @a */
    for (ol = b; ol; ol = ol->next) {
        if (!find_slot_option(a, ol->value->option)) {
            return false;
        }
    }

    for (ol = a; ol; ol = ol->next) {
        SlotOption *ao = ol->value;
        SlotOption *bo = find_slot_option(b, ao->option);

        if (!bo) {
            return false;
        }

        if (qobject_compare(bo->values, ao->values)) {
            if (mismatch && strcmp(mismatch, ao->option)) {
                return false;
            }

            mismatch = ao->option;
        }
    }

    if (opt_name) {
        *opt_name = mismatch;
    }
    return true;
}

static int compare_strList(strList *a, strList *b)
{
    for (; a && b; a = a->next, b = b->next) {
        int c = strcmp(a->value, b->value);
        if (c) {
            return c;
        }
    }

    if (b) {
        return -1;
    } else if (a) {
        return 1;
    } else {
        return 0;
    }

}

bool slots_can_be_combined(DeviceSlotInfo *a, DeviceSlotInfo *b,
                           const char **opt_name)
{
    if (a->available != b->available ||
        a->hotpluggable != b->hotpluggable ||
        a->has_count != b->has_count ||
        a->opts_complete != b->opts_complete ||
        a->has_device || b->has_device ||
        compare_strList(a->device_types, b->device_types)) {
        return false;
    }

    return slot_options_can_be_combined(a->opts, b->opts, opt_name);
}

void slots_combine(DeviceSlotInfo *a, DeviceSlotInfo *b, const char *opt_name)
{
    assert(slots_can_be_combined(a, b, NULL));
    if (a->has_count) {
        a->count += b->count;
    }
    if (opt_name) {
        SlotOption *aopt = find_slot_option(a->opts, opt_name);
        SlotOption *bopt = find_slot_option(b->opts, opt_name);

        qobject_incref(bopt->values);
        valuelist_extend(&aopt->values, bopt->values);
    }

    qapi_free_DeviceSlotInfo(b);
}

bool slots_try_combine(DeviceSlotInfo *a, DeviceSlotInfo *b)
{
    const char *opt = NULL;
    if (slots_can_be_combined(a, b, &opt)) {
        slots_combine(a, b, opt);
        return true;
    }

    return false;
}

void slot_list_add_slot(DeviceSlotInfoList **l, DeviceSlotInfo *slot)
{
	DeviceSlotInfoList *li;

    /* Try to combine slot with existing ones, if possible */
	for (li = *l; li; li = li->next) {
		if (slots_try_combine(li->value, slot)) {
			return;
		}
	}

	li = g_new0(DeviceSlotInfoList, 1);
	li->value = slot;
	li->next = *l;
	*l = li;
}

void slot_add_prop(DeviceSlotInfo *slot, const char *option, QObject *values)
{
    SlotOptionList *l =  g_new0(SlotOptionList, 1);

    l->value = g_new0(SlotOption, 1);
    l->value->option = g_strdup(option);
    l->value->values = values;
    l->next = slot->opts;
    slot->opts = l;
}

/*TODO: move it to common code */
static inline bool qbus_is_full(BusState *bus)
{
    BusClass *bus_class = BUS_GET_CLASS(bus);
    return bus_class->max_dev && bus->max_index >= bus_class->max_dev;
}

DeviceSlotInfo *make_slot(BusState *bus)
{
    DeviceSlotInfo *s = g_new0(DeviceSlotInfo, 1);

    s->device_types = g_new0(strList, 1);
    s->device_types->value = g_strdup(BUS_GET_CLASS(bus)->device_type);

    s->hotpluggable = qbus_is_hotpluggable(bus);

    /* Conditions that make a bus unavailable:
     * - Bus already full
     * - Hotplug when the bus is not hotpluggable
     */
    s->available =
        !(qbus_is_full(bus) ||
          (qdev_hotplug && !qbus_is_hotpluggable(bus)));


    /* s->opts = { 'bus': bus->name } */
    slot_add_prop_str(s, "bus", bus->name);

    return s;
}

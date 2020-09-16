#ifndef INTC_H
#define INTC_H

#include "qom/object.h"

#define TYPE_INTERRUPT_STATS_PROVIDER "intctrl"

typedef struct InterruptStatsProviderClass InterruptStatsProviderClass;
typedef struct InterruptStatsProvider InterruptStatsProvider;
DECLARE_INTERFACE_CHECKERS(InterruptStatsProvider, InterruptStatsProviderClass,
                           INTERRUPT_STATS_PROVIDER, TYPE_INTERRUPT_STATS_PROVIDER)


struct InterruptStatsProviderClass {
    InterfaceClass parent;

    /* The returned pointer and statistics must remain valid until
     * the BQL is next dropped.
     */
    bool (*get_statistics)(InterruptStatsProvider *obj, uint64_t **irq_counts,
                           unsigned int *nb_irqs);
    void (*print_info)(InterruptStatsProvider *obj, Monitor *mon);
};

#endif

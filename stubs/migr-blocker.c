#include "qemu/osdep.h"
#include "qemu-common.h"
#include "migration/blocker.h"

int migrate_add_blocker(Error *reason, Error *errp[static 1])
{
    return 0;
}

void migrate_del_blocker(Error *reason)
{
}

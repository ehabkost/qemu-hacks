#include "qemu/osdep.h"
#include "qemu-common.h"
#include "sysemu/arch_init.h"
#include "qapi/qmp/qerror.h"

CpuModelCompareInfo *arch_query_cpu_model_comparison(CpuModelInfo *modela,
                                                     CpuModelInfo *modelb,
                                                     Error *errp[static 1])
{
    error_setg(errp, QERR_UNSUPPORTED);
    return NULL;
}

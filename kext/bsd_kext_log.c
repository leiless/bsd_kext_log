/*
 * Created 190414 lynnl
 */

#include <mach/mach_types.h>

#include "utils.h"
#include "log_kctl.h"

kern_return_t bsd_kext_log_start(kmod_info_t *ki, void *d)
{
    UNUSED(ki, d);
    kern_return_t r;
    r = log_kctl_register() == 0 ? KERN_SUCCESS : KERN_FAILURE;
    return r;
}

kern_return_t bsd_kext_log_stop(kmod_info_t *ki, void *d)
{
    UNUSED(ki, d);
    return KERN_SUCCESS;
}


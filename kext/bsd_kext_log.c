/*
 * Created 190414 lynnl
 */

#include <mach/mach_types.h>

#include "utils.h"
#include "kauth.h"
#include "log_kctl.h"

kern_return_t bsd_kext_log_start(kmod_info_t *ki, void *d)
{
    kern_return_t r;

    UNUSED(ki, d);

    r = log_kctl_register();
    if (r) goto out_exit;

    r = kauth_register();
    if (r) {
        /* XXX: what if a client connected in the interim(is it possible?) */
        (void) log_kctl_deregister();
        goto out_exit;
    }

out_exit:
    return r;
}

kern_return_t bsd_kext_log_stop(kmod_info_t *ki, void *d)
{
    kern_return_t r;

    UNUSED(ki, d);

    kauth_deregister();

    r = log_kctl_deregister();

    return r;
}


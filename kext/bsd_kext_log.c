/*
 * Created 190414 lynnl
 */

#include <mach/mach_types.h>

#include "utils.h"
#include "kauth.h"
#include "log_kctl.h"
#include "log_sysctl.h"

kern_return_t bsd_kext_log_start(kmod_info_t *ki, void *d)
{
    kern_return_t r;

    UNUSED(ki, d);

    log_sysctl_register();

    r = kauth_register();
    if (r) goto out_exit;

    r = log_kctl_register();
    if (r) {
        kauth_deregister();
        goto out_exit;
    }

out_exit:
    return r;
}

kern_return_t bsd_kext_log_stop(kmod_info_t *ki, void *d)
{
    kern_return_t r;

    UNUSED(ki, d);

    r = log_kctl_deregister();
    if (r == KERN_SUCCESS) {
        kauth_deregister();
        log_sysctl_deregister();
        util_massert();
    }

    return r;
}

#ifdef __kext_makefile__
extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);

/* Will expand name if it's a macro */
#define KMOD_EXPLICIT_DECL2(name, ver, start, stop) \
    __attribute__((visibility("default")))          \
        KMOD_EXPLICIT_DECL(name, ver, start, stop)

KMOD_EXPLICIT_DECL2(BUNDLEID, KEXTBUILD_S, _start, _stop)

/* If you intended to write a kext library  NULLify _realmain and _antimain */
__private_extern__ kmod_start_func_t *_realmain = bsd_kext_log_start;
__private_extern__ kmod_stop_func_t *_antimain = bsd_kext_log_stop;

__private_extern__ int _kext_apple_cc = __APPLE_CC__;
#endif


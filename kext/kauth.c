/*
 * Created 190415 lynnl
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/vnode.h>

#include "kauth.h"
#include "utils.h"
#include "log_kctl.h"

static int vnode_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{

    return KAUTH_RESULT_DEFER;
}

static kauth_listener_t vnode_scope_ref = NULL;

errno_t kauth_register(void)
{
    errno_t e = 0;
    vnode_scope_ref = kauth_listen_scope(KAUTH_SCOPE_VNODE, vnode_scope_cb, NULL);
    if (vnode_scope_ref == NULL) {
        e = ENOMEM;
        log_error("kauth_listen_scope() fail  scope: %s", KAUTH_SCOPE_VNODE);
    }
    return e;
}


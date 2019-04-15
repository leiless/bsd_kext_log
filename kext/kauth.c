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

typedef struct {
    errno_t e;
    int len;        /* strlen(path) */
    char *path;
} vnode_path_t;

/**
 * Get path of vnode(one vnode may have more than one path)
 * @vp          a vnode pointer
 * @return      a struct contains info about the vnode path
 *              XXX: must check vnode_path_t.e before use
 *              you're responsible to free vnode_path_t.path after use
 */
static vnode_path_t make_vnode_path(vnode_t vp)
{
    int e;
    int len = PATH_MAX;     /* Don't touch */
    char *path = NULL;

    kassert_nonnull(vp);

    /*
     * NOTE:
     *  For compatibility reason
     *  Length of the path should(and must) be PATH_MAX(1024 bytes)
     *
     * References:
     *  developer.apple.com/legacy/library/technotes/tn/tn1150.html#Symlinks
     */
    path = (char *) _MALLOC(PATH_MAX, M_TEMP, M_NOWAIT);
    if (path == NULL) {
        e = ENOMEM;
        goto out_exit;
    }

    /*
     * There must be a NULL-terminator inside *path  don't worry
     *  len = strlen(path) + 1(EOS) in result
     *
     * NOTE:
     *  The third parameter of vn_getpath() must be initialized
     *  O.w. kernel will panic  see: xnu/bsd/vfs/vfs_subr.c
     */
    e = vn_getpath(vp, path, &len);
    if (e == 0) {
        kassertf(len > 0, "non-positive len %d", len);
        len--;      /* Don't count trailing '\0' */
    } else {
        _FREE(path, M_TEMP);
        path = NULL;
    }

out_exit:
    return (vnode_path_t) {e, len, path};
}

static int vnode_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    vfs_context_t ctx;
    vnode_t vp;
    vnode_t dvp;

    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    vnode_path_t vpath;

    if (kcb_get() < 0) goto out_put;

    kassertf(idata == NULL, "idata %p?!", idata);

    ctx = (vfs_context_t) arg0;
    UNUSED(ctx);
    vp = (vnode_t) arg1;
    dvp = (vnode_t) arg2;           /* may NULLVP(alias of NULL) */

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    vpath = make_vnode_path(vp);
    if (vpath.e != 0) {
        log_error("make_vnode_path() fail  vp: %p vid: %#x vt: %d",
                    vp, vnode_vid(vp), vnode_vtype(vp));
        goto out_put;
    }

    UNUSED(arg3);   /* XXX: TODO */

    log_info("vnode  act: %#x dvp: %p vp: %p %#x %d %s uid: %u pid: %s %d",
                act, dvp, vp, vnode_vid(vp), vnode_vtype(vp),
                vpath.path, uid, pcomm, pid);

    _FREE(vpath.path, M_TEMP);
out_put:
    (void) kcb_put();
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


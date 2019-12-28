/*
 * Created 190415 lynnl
 *
 * see:
 *  https://developer.apple.com/library/archive/technotes/tn2127/_index.html
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

static int generic_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg0, arg1, arg2, arg3);

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    log_info("generic  act: %#x uid: %u pid: %d %s", act, uid, pid, pcomm);

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

static int process_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    proc_t proc;
    int pid2;
    char pcomm2[MAXCOMLEN + 1];
    int signal;

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg2, arg3);

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    switch (act) {
    case KAUTH_PROCESS_CANTRACE:
        proc = (proc_t) arg0;
        pid2 = proc_pid(proc);
        proc_name(pid2, pcomm2, sizeof(pcomm2));

        log_info("process  act: %#x(can_trace) uid: %u pid: %d %s dst: %d %s",
                    act, uid, pid, pcomm, pid2, pcomm2);
        break;

    case KAUTH_PROCESS_CANSIGNAL:
        proc = (proc_t) arg0;
        signal = (int) arg1;
        pid2 = proc_pid(proc);
        proc_name(pid2, pcomm2, sizeof(pcomm2));

        log_info("process  act: %#x(can_signal) uid: %u pid: %d %s dst: %d %s sig: %d",
                    act, uid, pid, pcomm, pid2, pcomm2, signal);
        break;

    default:
        panicf("unknown action %#x in process scope", act);
        __builtin_unreachable();
    }

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
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

    UNUSED(idata, arg3);   /* XXX: TODO? */

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

    log_info("vnode  act: %#x dvp: %p vp: %p %d %s uid: %u pid: %d %s",
                act, dvp, vp, vnode_vtype(vp), vpath.path, uid, pid, pcomm);

    _FREE(vpath.path, M_TEMP);
out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

/*
 * [sic Technical Note TN2127 Kernel Authorization#File Operation Scope]
 *
 * Warning:
 * Prior to Mac OS X 10.5 the file operation scope had a nasty gotcha(r. 4605516).
 * If you install a listener in the this scope and handle the
 *      KAUTH_FILEOP_RENAME,
 *      KAUTH_FILEOP_LINK, or
 *      KAUTH_FILEOP_EXEC actions,
 *  you must test whether arg0 and arg1 are NULL before accessing them as strings.
 *
 * Under certain circumstances(most notably, very early in the boot sequence
 *  and very late in the shutdown sequence),
 *  the kernel might pass you NULL for these arguments.
 *
 * If you access such a pointer as a string, you will kernel panic.
 */
static int fileop_scope_cb(
        kauth_cred_t cred,
        void *idata,
        kauth_action_t act,
        uintptr_t arg0,
        uintptr_t arg1,
        uintptr_t arg2,
        uintptr_t arg3)
{
    uid_t uid;
    int pid;
    char pcomm[MAXCOMLEN + 1];

    vnode_t vp;
    const char *path1;
    const char *path2;
    int flags;

    if (kcb_get() < 0) goto out_put;

    UNUSED(idata, arg3);

    uid = kauth_cred_getuid(cred);
    pid = proc_selfpid();
    proc_selfname(pcomm, sizeof(pcomm));

    switch (act) {
    case KAUTH_FILEOP_OPEN:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;

        log_info("fileop  act: %#x(open) vp: %p %d %s uid: %u pid: %d %s",
                    act, vp, vnode_vtype(vp), path1, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_CLOSE:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;
        flags = (int) arg2;

        log_info("fileop  act: %#x(close) vp: %p %d %s flags: %#x uid: %u pid: %d %s",
                    act, vp, vnode_vtype(vp), path1, flags, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_RENAME:
        path1 = (char * _Nullable) arg0;
        path2 = (char * _Nullable) arg1;

        log_info("fileop  act: %#x(rename) %s -> %s uid: %u pid: %d %s",
                    act, path1, path2, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_EXCHANGE:
        path1 = (char *) arg0;
        path2 = (char *) arg1;

        log_info("fileop  act: %#x(xchg) %s <=> %s uid: %u pid: %d %s",
                    act, path1, path2, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_LINK:
        path1 = (char * _Nullable) arg0;
        path2 = (char * _Nullable) arg1;

        log_info("fileop  act: %#x(link) %s ~> %s uid: %u pid: %d %s",
                    act, path1, path2, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_EXEC:
        vp = (vnode_t) arg0;
        path1 = (char * _Nullable) arg1;

        log_info("fileop  act: %#x(exec) vp: %p %d %s uid: %u pid: %d %s",
                    act, vp, vnode_vtype(vp), path1, uid, pid, pcomm);
        break;

    case KAUTH_FILEOP_DELETE:
        vp = (vnode_t) arg0;
        path1 = (char *) arg1;

        log_info("fileop  act: %#x(del) vp: %p %d %s uid: %u pid: %d %s",
                    act, vp, vnode_vtype(vp), path1, uid, pid, pcomm);
        break;

    default:
        panicf("unknown action %#x in fileop scope", act);
        __builtin_unreachable();
    }

out_put:
    (void) kcb_put();
    return KAUTH_RESULT_DEFER;
}

static const char *scope_name[] = {
    KAUTH_SCOPE_GENERIC,
    KAUTH_SCOPE_PROCESS,
    KAUTH_SCOPE_VNODE,
    KAUTH_SCOPE_FILEOP,
};

static kauth_scope_callback_t scope_cb[] = {
    generic_scope_cb,
    process_scope_cb,
    vnode_scope_cb,
    fileop_scope_cb,
};

static kauth_listener_t scope_ref[] = {
    NULL, NULL, NULL, NULL,
};

kern_return_t kauth_register(void)
{
    kern_return_t r = KERN_SUCCESS;
    int i;

    BUILD_BUG_ON(ARRAY_SIZE(scope_name) != ARRAY_SIZE(scope_cb));
    BUILD_BUG_ON(ARRAY_SIZE(scope_name) != ARRAY_SIZE(scope_ref));

    for (i = 0; i < (int) ARRAY_SIZE(scope_name); i++) {
        scope_ref[i] = kauth_listen_scope(scope_name[i], scope_cb[i], NULL);
        if (scope_ref[i] == NULL) {
            r = KERN_FAILURE;
            log_error("kauth_listen_scope() fail  scope: %s", scope_name[i]);
            kauth_deregister();
            break;
        }
    }

    return r;
}

void kauth_deregister(void)
{
    int i;

    for (i = 0; i < (int) ARRAY_SIZE(scope_name); i++) {
        if (scope_ref[i] != NULL) {
            kauth_unlisten_scope(scope_ref[i]);
            scope_ref[i] = NULL;
        }
    }

    kcb_invalidate();
}


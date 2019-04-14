/*
 * Created 190414 lynnl
 */

#include <sys/systm.h>
#include <sys/errno.h>
#include <libkern/OSAtomic.h>

#include "log_kctl.h"
#include "utils.h"
#include "kextlog.h"

#define LOG_KCTL_NAME       "net.tty4.kext.kctl.log"

static errno_t log_kctl_connect( kern_ctl_ref, struct sockaddr_ctl *, void **);
static errno_t log_kctl_disconnect(kern_ctl_ref, u_int32_t, void *);

static struct kern_ctl_reg kctlreg = {
    LOG_KCTL_NAME,          /* ctl_name */
    0,                      /* ctl_id */
    0,                      /* ctl_unit */
    0,                      /* ctl_flags */
    0,                      /* ctl_sendsize */
    0,                      /* ctl_recvsize */
    log_kctl_connect,       /* ctl_connect */
    log_kctl_disconnect,    /* ctl_disconnect */
    NULL,                   /* ctl_send */
    NULL,                   /* ctl_setopt */
    NULL,                   /* ctl_getopt */
};

static kern_ctl_ref kctlref = NULL;
static volatile u_int32_t kctlunit = 0;     /* Active unit is positive */

static errno_t log_kctl_connect(
        kern_ctl_ref ref,
        struct sockaddr_ctl *sac,
        void **unitinfo)
{
    errno_t e = 0;

    BUILD_BUG_ON(sizeof(u_int32_t) != sizeof(UInt32));

    kassert(ref == kctlref);

    if (OSCompareAndSwap(0, sac->sc_unit, (UInt32 *) &kctlunit)) {
        kassert_nonnull(unitinfo);
        *unitinfo = NULL;
        LOG_DBG("Log kctl connected  unit: %u", sac->sc_unit);
    } else {
        e = EISCONN;
        LOG_WARN("Log kctl already connected  skip");
    }

    return e;
}

static errno_t log_kctl_disconnect(
        kern_ctl_ref ref,
        u_int32_t unit,
        void *unitinfo)
{
    UNUSED(ref);
    if (OSCompareAndSwap(unit, 0, &kctlunit)) {
        kassert(unitinfo == NULL);
        LOG_DBG("Log kctl client disconnected  unit: %u", unit);
    } else {
        /* Refused clients */
    }
    return 0;
}

errno_t log_kctl_register(void)
{
    errno_t e = ctl_register(&kctlreg, &kctlref);
    if (e == 0) {
        LOG_DBG("kctl %s registered  ref: %p", LOG_KCTL_NAME, kctlref);
    } else {
        LOG_ERR("ctl_register() fail  errno: %d", e);
    }
    return e;
}

errno_t log_kctl_deregister(void)
{
    errno_t e = 0;
    /* ctl_deregister(NULL) returns EINVAL */
    e = ctl_deregister(kctlref);
    if (e == 0) {
        LOG_DBG("kctl %s deregistered  ref: %p", LOG_KCTL_NAME, kctlref);
    } else {
        LOG_ERR("ctl_deregister() fail  ref: %p errno: %d", kctlref, e);
    }
    return e;
}

void log_printf(uint32_t level, const char *fmt, ...)
{
    if (kctlunit == 0) return;

    struct kextlog_stackmsg msg;
    struct kextlog_msghdr *msgp;
    int len;
    int len2;
    va_list ap;
    uint32_t msgsz;
    uint32_t flags = 0;

out_again:
    msgp = (struct kextlog_msghdr *) &msg;

    va_start(ap, fmt);
    /*
     * vsnprintf() return the number of characters that would have been printed
     *  if the size were unlimited(not including the final `\0')
     *
     * return value of vsnprintf() always non-negative
     */
    len = vsnprintf(msg.buffer, sizeof(msg.buffer), fmt, ap);
    va_end(ap);

    if (__builtin_uadd_overflow(sizeof(*msgp), len, &msgsz) ||
        /* Includes log message trailing `\0' */
        __builtin_uadd_overflow(msgsz, 1, &msgsz)) {
        /* Should never happen */
        LOG_ERR("log_printf() message size overflow  level: %u fmt: %s msgsz: %u", level, fmt, msgsz);
        goto out_overflow;
    }

    if (len >= (int) sizeof(msg.buffer)) {
        msgp = (struct kextlog_msghdr *) _MALLOC(msgsz, M_TEMP, M_NOWAIT);
        if (msgp != NULL) {
            va_start(ap, fmt);
            len2 = vsnprintf(msgp->buffer, len + 1, fmt, ap);
            va_end(ap);

            if (len2 > len) {
                /* TOCTOU: Some arguments got modified in the interim */
                _FREE(msgp, M_TEMP);
                goto out_again;
            }
        } else {
            msgp = (struct kextlog_msghdr *) &msg;
out_overflow:
            msgsz = sizeof(msg);
            flags |= KEXTLOG_FLAG_MSG_TRUNCATED;
        }
    }

    /* TODO: send log message into user space */

    if (msgp != (struct kextlog_msghdr *) &msg) {
        /* _FREE(NULL, type) do nop */
        _FREE(msgp, M_TEMP);
    }
}


/*
 * Created 190417 lynnl
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>

#include "../kext/kextlog.h"

#define LOG(fmt, ...)       (void) printf(fmt "\n", ##__VA_ARGS__)
#define LOG_OFF(fmt, ...)   (void) ((void) (fmt), ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   (void) fprintf(stderr, "[ERR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  (void) fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define LOG_DBG(fmt, ...)   LOG("[DBG] " fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)   LOG_OFF(fmt, ##__VA_ARGS__)
#endif

/**
 * Connect to a kernel control
 * @name        kernel control name
 * @socktype    either SOCK_DGRAM or SOCK_STREAM
 * @return      -1 if failed(errno will be set)  fd otherwise
 */
static int connect_to_kctl(const char *name, int socktype)
{
    int fd = -1;
    int e;
    struct sockaddr_ctl sc;
    struct ctl_info ci;

    if (strlen(name) >= MAX_KCTL_NAME) {
        LOG_ERR("kernel control socket name too long");
        goto out_exit;
    }

    fd = socket(PF_SYSTEM, socktype, SYSPROTO_CONTROL);
    if (fd == -1) {
        LOG_ERR("socket(2) fail  errno: %d", errno);
        goto out_exit;
    }

    LOG_DBG("socket fd: %d", fd);

    (void) memset(&ci, 0, sizeof(struct ctl_info));
    (void) strncpy(ci.ctl_name, name, MAX_KCTL_NAME-1);
    ci.ctl_name[MAX_KCTL_NAME-1] = '\0';

    /*
     * The control ID is dynamically generated
     * So we must obtain ci.sc_id via ioctl()
     *
     * If ioctl returns ENOENT  it means that socket descriptor is nonexist
     * So before we run the daemon  we must load kext to the kernel
     */
    e = ioctl(fd, CTLIOCGINFO, &ci);
    if (e == -1) {
        LOG_ERR("ioctl(2) CTLIOCGINFO fail  name: %s fd: %d errno: %d",
                    ci.ctl_name, fd, errno);
        goto out_close;
    }

    LOG_DBG("kctl %s id %d", ci.ctl_name, ci.ctl_id);

    (void) memset(&sc, 0, sizeof(sc));
    sc.sc_len = sizeof(struct sockaddr_ctl);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_id = ci.ctl_id;
    sc.sc_unit = 0;

    e = connect(fd, (struct sockaddr *) &sc, sizeof(struct sockaddr_ctl));
    if (e == -1) {
        LOG_ERR("connect(2) fail  fd: %d", fd);
        goto out_close;
    }

    LOG("kctl %s connected  fd: %d", ci.ctl_name, fd);

out_exit:
    return fd;

out_close:
    (void) close(fd);
    fd = -1;
    goto out_exit;
}

#define BUFFER_SIZE     8192

static char message[BUFFER_SIZE];

static void read_log_from_kctl(int fd)
{
    struct kextlog_msghdr msg;
    ssize_t n;
    char *p;

out_read:
    while (1) {
        n = read(fd, &msg, sizeof(msg));
        if (n < 0) {
            if (errno == EINTR) goto out_read;
            LOG_ERR("read(2) fail  errno: %d", errno);
            break;
        } else if (n != sizeof(msg)) {
            LOG_ERR("expected size %zu  got %zd", sizeof(msg), n);
            break;
        }

        LOG("level: %u flags: %#x ts: %#llx sz: %u",
            msg.level, msg.flags, msg.timestamp, msg.size);

        if (msg.size <= n) {
            p = message;
        } else {
            p = (char *) malloc(msg.size);
            if (p == NULL) {
                LOG_ERR("malloc(3) fail  size: %u errno: %d", msg.size, errno);
                break;
            }
        }

out_read2:
        n = read(fd, p, msg.size);
        if (n < 0) {
            if (errno == EINTR) goto out_read2;
            LOG_ERR("read(2) fail  errno: %d", errno);
            break;
        } else if (n != msg.size) {
            LOG_ERR("expected size %u  got %zd", msg.size, n);
            break;
        }

        LOG("%.*s", (int) n, p);

        if (p != message) free(p);
    }
}

#define KEXTLOG_KCTL_NAME       "net.tty4.kext.kctl.log"
#define KEXTLOG_KCTL_SOCKTYPE   SOCK_DGRAM

int main(int argc, char *argv[])
{
    int fd = connect_to_kctl(KEXTLOG_KCTL_NAME, KEXTLOG_KCTL_SOCKTYPE);
    if (fd >= 0) {
        read_log_from_kctl(fd);
        (void) close(fd);
    }
    return 0;
}


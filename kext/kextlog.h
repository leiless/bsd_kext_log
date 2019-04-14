/*
 * Created 190414 lynnl
 */

#ifndef KEXTLOG_H
#define KEXTLOG_H

#include <stdint.h>

#define KEXTLOG_LEVEL_INFO          0
#define KEXTLOG_LEVEL_DEBUG         1
#define KEXTLOG_LEVEL_WARNING       2
#define KEXTLOG_LEVEL_ERROR         3
#define KEXTLOG_LEVEL_TRACE         4

/*
 * Indicate direct-previous messages dropped due to failure
 */
#define KEXTLOG_FLAG_MSG_DROPPED    0x1
#define KEXTLOG_FLAG_MSG_TRUNCATED  0x2

struct kextlog_msghdr {
    uint32_t level;
    uint32_t flags;
    uint64_t timestamp;     /* always be mach_absolute_time() */

    uint32_t size;          /* Size of message buffer */

    /*
     * Zero size must be given to silence
     *  -Wgnu-variable-sized-type-not-at-end for nested structs
     */
    char buffer[0];
};

#define KEXTLOG_STACKMSG_SIZE       128

struct kextlog_stackmsg {
    struct kextlog_msghdr hdr;
    char buffer[KEXTLOG_STACKMSG_SIZE];
};

#endif /* KEXTLOG_H */


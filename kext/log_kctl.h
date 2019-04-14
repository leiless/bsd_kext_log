/*
 * Created 190414 lynnl
 *
 * Implements kernel control socket functionalities
 */

#ifndef LOG_KCTL_H
#define LOG_KCTL_H

#include <sys/kern_control.h>

errno_t log_kctl_register(void);
errno_t log_kctl_deregister(void);

void log_printf(uint32_t, const char *, ...) __printflike(2, 3);

#endif /* LOG_KCTL_H */


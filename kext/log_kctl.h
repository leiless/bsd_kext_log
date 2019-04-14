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

#endif /* LOG_KCTL_H */


/*
 * Created 190415 lynnl
 *
 * Implementation of KAuth callbacks
 */

#ifndef KAUTH_H
#define KAUTH_H

#include <sys/systm.h>

kern_return_t kauth_register(void);
void kauth_deregister(void);

#endif /* KAUTH_H */


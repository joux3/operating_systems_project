#ifdef CHANGED_1

#ifndef BUENOS_KERNEL_LOCK_COND_H
#define BUENOS_KERNEL_LOCK_COND_H

#include "kernel/spinlock.h"

typedef struct {
  spinlock_t slock;
  int thread_count;
  int created;
} lock_t;

typedef struct {
  spinlock_t slock;
  int created;
} cond_t;

void lock_cond_init(void);

lock_t *lock_create(void);
void lock_destroy(lock_t *lock);
void lock_acquire(lock_t *lock);
void lock_release(lock_t *lock);

cond_t *condition_create(void);
void condition_destroy(cond_t *cond);
void condition_wait(cond_t *cond, lock_t *condition_lock);
void condition_signal(cond_t *cond, lock_t *condition_lock);
void condition_broadcast(cond_t *cond, lock_t *condition_lock);

#endif

#endif

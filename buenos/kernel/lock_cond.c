#ifdef CHANGED_1

#include "kernel/lock_cond.h"

void lock_cond_init(void) {
}

lock_t *lock_create(void) {
  return 0;
}
void lock_destroy(lock_t *lock) {}
void lock_acquire(lock_t *lock) {}
void lock_release(lock_t *lock) {}

cond_t *condition_create(void) {
  return 0;
}
void condition_destroy(cond_t *cond) {}
void condition_wait(cond_t *cond, lock_t *condition_lock) {}
void condition_signal(cond_t *cond, lock_t *condition_lock) {}
void condition_broadcast(cond_t *cond, lock_t *condition_lock) {}

#endif

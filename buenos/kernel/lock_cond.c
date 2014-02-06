#ifdef CHANGED_1

#include "kernel/interrupt.h"
#include "kernel/sleepq.h"
#include "kernel/lock_cond.h"
#include "kernel/config.h"
#include "kernel/assert.h"
#include "kernel/thread.h"
#include "lib/libc.h"

/** Table containing all locks in the system */
static lock_t lock_table[CONFIG_MAX_LOCKS];

/** Lock which must be held before accessing the lock_table */
static spinlock_t lock_table_slock;

/** Table containing all condition variables in the system */
static cond_t cond_table[CONFIG_MAX_CONDITION_VARIABLES];

/** Lock which must be held before accessing the cond_table */
static spinlock_t cond_table_slock;

void lock_cond_init(void) {
    int i;

    spinlock_reset(&lock_table_slock);
    for(i = 0; i < CONFIG_MAX_LOCKS; i++)
        lock_table[i].created = 0;

    spinlock_reset(&cond_table_slock);
    for(i = 0; i < CONFIG_MAX_CONDITION_VARIABLES; i++)
        cond_table[i].created = 0;
}

lock_t *lock_create(void) {
    interrupt_status_t intr_status;
    static int next = 0;
    int i;
    int lock_id;

    intr_status = _interrupt_disable();
    spinlock_acquire(&lock_table_slock);

    // find a free lock
    for(i = 0; i < CONFIG_MAX_LOCKS; i++) {
        lock_id = next;
        next = (next + 1) % CONFIG_MAX_LOCKS;
        if (lock_table[lock_id].created == 0) {
            lock_table[lock_id].created = 1;
            break;
        }
    }

    spinlock_release(&lock_table_slock);
    _interrupt_set_state(intr_status);

    if (i == CONFIG_MAX_LOCKS) {
	/* lock table does not have any free locks, creation fails */
        return NULL;
    }

    lock_table[lock_id].thread_count = 0;
    spinlock_reset(&lock_table[lock_id].slock);

    return &lock_table[lock_id];
}

void lock_destroy(lock_t *lock) {
    lock->created = 0; 
}

void lock_acquire(lock_t *lock) {
    interrupt_status_t intr_status;

    intr_status = _interrupt_disable();
    spinlock_acquire(&lock->slock);

    lock->thread_count++;
    if (lock->thread_count > 1) {
        sleepq_add(lock);
        spinlock_release(&lock->slock);
        thread_switch();
    } else {
        spinlock_release(&lock->slock);
    }
    _interrupt_set_state(intr_status);
}

void lock_release(lock_t *lock) {
    interrupt_status_t intr_status;
    
    intr_status = _interrupt_disable();
    spinlock_acquire(&lock->slock);

    lock->thread_count--;
    if (lock->thread_count > 1) {
        sleepq_wake(lock);
    }

    spinlock_release(&lock->slock);
    _interrupt_set_state(intr_status);
}

cond_t *condition_create(void) {
    interrupt_status_t intr_status;
    static int next = 0;
    int i;
    int cond_id;

    intr_status = _interrupt_disable();
    spinlock_acquire(&cond_table_slock);

    // find a free condition variable
    for(i = 0; i < CONFIG_MAX_CONDITION_VARIABLES; i++) {
        cond_id = next;
        next = (next + 1) % CONFIG_MAX_CONDITION_VARIABLES;
        if (cond_table[cond_id].created == 0) {
            cond_table[cond_id].created = 1;
            break;
        }
    }

    spinlock_release(&cond_table_slock);
    _interrupt_set_state(intr_status);

    if (i == CONFIG_MAX_CONDITION_VARIABLES) {
	/* cond table does not have any free condition variables, creation fails */
        return NULL;
    }

    spinlock_reset(&cond_table[cond_id].slock);

    return &cond_table[cond_id];
}

void condition_destroy(cond_t *cond) {
    cond->created = 0;
}

void condition_wait(cond_t *cond, lock_t *condition_lock) {
    interrupt_status_t intr_status;

    intr_status = _interrupt_disable();
    
    sleepq_add(cond);
    lock_release(condition_lock);
    thread_switch();

    lock_acquire(condition_lock);

    _interrupt_set_state(intr_status);
}

void condition_signal(cond_t *cond) {
    sleepq_wake(cond);
}

void condition_broadcast(cond_t *cond) {
    sleepq_wake_all(cond);
}

#endif

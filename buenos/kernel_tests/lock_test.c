#ifdef CHANGED_1

#include "kernel/lock_cond.h"
#include "kernel/thread.h"

lock_t *lock;

void lock_test_thread(uint32_t param) {
    kprintf("stared thread %d\n", param);
    while (1) {
        int j;
        lock_acquire(lock);
        kprintf("locked in thread %d\n", param);
        for (j = 0; j < 10000; j++) {
            thread_switch();
        }
        kprintf("freeing in thread %d\n", param);
        lock_release(lock);
    }
}

int lock_test_main(void)
{
    int i;
    kprintf("starting lock test\n");
    lock = lock_create();
    for (i = 0; i < 3; i++) {
        TID_t thread;
        thread = thread_create(&lock_test_thread, i);
        thread_run(thread);
    }
    while (1) {
    }

    return 0;
}

#endif

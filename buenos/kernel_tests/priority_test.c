#ifdef CHANGED_1

#include "kernel/thread.h"
#include "lib/libc.h"

void high_priority_thread(uint32_t param) {
    param++;
    while(1) {
        kprintf("Running high priority thread\n");
    }
}

void normal_priority_thread(uint32_t param) {
    param++;
    while(1) {
        kprintf("Running low priority thread\n");
    }
}

void priority_test_main(void) {
    kprintf("starting high priority threads\n");
    int i;
    for (i = 0; i < 4; i++) {
        TID_t VIP_thread;
        VIP_thread = thread_create_priority(&high_priority_thread, 1, PRIORITY_HIGH);
        thread_run(VIP_thread);
    }
    for (i = 0; i < 100; i++) {
        TID_t normal_thread;
        kprintf("starting normal priority threads\n");
        normal_thread = thread_create_priority(&normal_priority_thread, 1, PRIORITY_NORMAL);
        thread_run(normal_thread);
    }
    while(1) {}
}

#endif

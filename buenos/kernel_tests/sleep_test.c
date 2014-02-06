#ifdef CHANGED_1

#include "kernel/lock_cond.h"
#include "kernel/thread.h"
#include "lib/libc.h"
#include "drivers/metadev.h"

void sleep_test_thread(uint32_t param) {
    param += 1;
    while (1) {
        uint32_t sleep_ms;
        uint32_t start_time;
        uint32_t end_time;
        uint32_t slept_time;
        start_time = rtc_get_msec();
        sleep_ms = 20000 + _get_rand(30000);
        kprintf("> sleeping in thread %d for %d, start_time %d\n", thread_get_current_thread(), sleep_ms, start_time);

        thread_sleep(sleep_ms);
        
        end_time = rtc_get_msec();
        slept_time = end_time - start_time;
        kprintf("> slept in thread %d for %d, asked for %d, now %d\n", thread_get_current_thread(), slept_time, sleep_ms, end_time);
    }
}

int sleep_test_main(void)
{
    int i;
    kprintf("starting sleep test\n");
    for (i = 0; i < 3; i++) {
        TID_t thread;
        thread = thread_create(&sleep_test_thread, i);
        thread_run(thread);
    }
    sleep_test_thread(0); 

    return 0;
}

#endif

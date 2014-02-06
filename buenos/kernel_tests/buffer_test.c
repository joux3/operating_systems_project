#ifdef CHANGED_1

#include "kernel/lock_cond.h"
#include "kernel/thread.h"
#include "lib/debug.h"

#define N 128
static uint8_t buffer[N];
static int start = 0;
static int end = 0;

lock_t *lock;
cond_t *notfull;
cond_t *notempty;

void buffer_put(uint8_t value) {
    DEBUG("buffer_test_debug","trying to acquire lock for writer thread %d\n", start, end, thread_get_current_thread());
    lock_acquire(lock);
    DEBUG("buffer_test_debug","acquired lock for writer thread %d\n", start, end, thread_get_current_thread());
    while (end == (start - 1) % N) {
        DEBUG("buffer_test_debug","start %d, end %d in writer thread %d\n", start, end, thread_get_current_thread());
        condition_wait(notfull, lock);
    }
    buffer[end] = value;
    end = (end + 1) % N;
    DEBUG("buffer_test_debug","wrote start %d, end %d in writer thread %d\n", start, end, thread_get_current_thread());
    lock_release(lock);
    DEBUG("buffer_test_debug","wrote start %d, end %d in writer thread %d\n", start, end, thread_get_current_thread());
    condition_signal(notempty);
}

uint8_t buffer_get(void) {
    uint8_t byte;
    DEBUG("buffer_test_debug","trying to acquire lock for reader thread %d\n", thread_get_current_thread());
    lock_acquire(lock);
    DEBUG("buffer_test_debug","acquired lock for reader thread %d\n", thread_get_current_thread());
    while (start == end) {
        DEBUG("buffer_test_debug","start %d, end %d in reader thread %d\n", start, end, thread_get_current_thread());
        condition_wait(notempty, lock);
    }
    byte = buffer[start];
    start = (start + 1) % N;
    lock_release(lock);
    condition_signal(notfull);
    return byte;
}

void produce_thread(uint32_t param) {
    int i = 0;
    param += 1;
    while(1) {
        i = (i + 1) % 256;
        kprintf("writing %d in thread %d\n", i, thread_get_current_thread());
        buffer_put(i);
    }
}

void consume_thread(uint32_t param) {
    param += 1;
    while(1) {
        kprintf("read %d in thread %d\n", buffer_get(), thread_get_current_thread());
    }
}

void buffer_test_main(void) {
    int i;
    kprintf("In kernel buffer test\n");
    lock = lock_create();
    notfull = condition_create();
    notempty = condition_create();
    kprintf("notfull %d, notempty %d\n", notfull, notempty);

    for (i = 0; i < 10; i++) {
        TID_t thread;
        thread = thread_create(&consume_thread, 0);
        thread_run(thread);
    }
    TID_t thread;
    thread = thread_create(&produce_thread, 999);
    thread_run(thread);

    while(1) {}
}

#endif

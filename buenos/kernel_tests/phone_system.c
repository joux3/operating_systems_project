#ifdef CHANGED_1

#include "kernel/semaphore.h"
#include "kernel/thread.h"
#include "lib/libc.h"
#include "kernel/assert.h"

#define LINES_IN_SYSTEM 5
#define CALLERS_IN_SYSTEM 10
#define ANSWERERS_IN_SYSTEM 3
#define TALK_TIME_MS 1000

static int free_lines;
static semaphore_t *sem_lines;
static semaphore_t *sem_phones;

int make_call(void) {
    semaphore_P(sem_lines);
    if (free_lines > 0) {
        KERNEL_ASSERT(free_lines > 0);
        free_lines--;
    } else {
        KERNEL_ASSERT(free_lines == 0);
        semaphore_V(sem_lines);
        return 0;
    }
    semaphore_V(sem_lines);
    semaphore_V(sem_phones);
    //kprintf("calling thread %d starts talking\n", thread_get_current_thread());
    return 1;
}

void phone(void) {
    semaphore_P(sem_phones);
    KERNEL_ASSERT(free_lines < LINES_IN_SYSTEM);
    kprintf("answering thread %d starts talking\n", thread_get_current_thread());
    //thread_sleep(TALK_TIME_MS);
    semaphore_P(sem_lines);
    free_lines++;
    KERNEL_ASSERT(free_lines <= LINES_IN_SYSTEM);
    semaphore_V(sem_lines);
}

void calling_thread(uint32_t param) {
    param++;
    while(1) {
        kprintf("calling thread %d making a call\n", thread_get_current_thread());
        int success;
        success = make_call();
        if (success) {
            kprintf("calling thread %d succeeded in making a call\n",
                    thread_get_current_thread());
            //thread_sleep(TALK_TIME_MS);
        } else {
            kprintf("calling thread %d failed at making a call, lines full\n",
                    thread_get_current_thread());
        }
    }
}

void answering_thread(uint32_t param) {
    param++;
    while(1) {
        kprintf("answering thread %d ready to answer\n", thread_get_current_thread());
        phone();
        kprintf("answering thread %d finished answering\n", thread_get_current_thread());
    }
}

int phone_system_main(void)
{
    // Give the system 5 phone lines to work with
    free_lines = LINES_IN_SYSTEM;
    // Initialize the semaphores to 0
    sem_lines = semaphore_create(1);
    sem_phones = semaphore_create(0);
    int i;
    kprintf("starting phone system demo\n");
    kprintf("starting %d answering threads\n", ANSWERERS_IN_SYSTEM);
    for (i = 0; i < ANSWERERS_IN_SYSTEM; i++) {
        TID_t thread;
        thread = thread_create(&answering_thread, i);
        thread_run(thread);
    }
    kprintf("starting %d calling threads\n", CALLERS_IN_SYSTEM);
    for (i = 0; i < CALLERS_IN_SYSTEM; i++) {
        TID_t thread;
        thread = thread_create(&calling_thread, i);
        thread_run(thread);
    }
    while(1) {}
    return 0;
}

#endif

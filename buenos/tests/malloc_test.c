#include "lib.h"

int main(void) {
    prints("lets begin\n");
    void *ptr = malloc(100);
    char buf[10];
    prints("Allocated, ptr is ");
    itoa((uint32_t)ptr, buf);
    prints(buf);
    prints("\n");

    void *memlimit = syscall_memlimit(0);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    void* ptr2 = malloc(100);
    prints("Allocated, ptr2 is ");
    itoa((uint32_t)ptr2, buf);
    prints(buf);
    prints("\n");

    free(ptr);
    memlimit = syscall_memlimit(0);
    prints("Dallocated first block, memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    ptr = malloc(100);
    prints("Allocated, ptr is ");
    itoa((uint32_t)ptr, buf);
    prints(buf);
    prints("\n");

	memlimit = syscall_memlimit(0);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    free(ptr);
    memlimit = syscall_memlimit(0);
    prints("Dallocated, memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    return 0;
}

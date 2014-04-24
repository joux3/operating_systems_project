#include "lib.h"

int main(void) {
    prints("lets begin\n");
    uint32_t size = 1;
    
    while(1) {
        void *ptr = malloc(size);
        char buf[10];
        if(ptr != 0) {
            prints("Allocated, ptr is ");
            itoa((uint32_t)ptr, buf);
            prints(buf);
            prints("\n");
        }
        else {
            prints("Alloc failed for size\n");
            itoa((uint32_t)size, buf);
            prints(buf);
            prints("\n");
            ptr = malloc(500);
            if(ptr != 0) {
                prints("Last test allocated, ptr is ");
                itoa((uint32_t)ptr, buf);
                prints(buf);
                prints("\n");

                void *memlimit = syscall_memlimit(0);
                prints("Current memlimit is ");
                itoa((int)memlimit, buf);
                prints(buf);
                prints("\n");
            }
            else {
                prints("Last alloc failed for size\n");
                itoa((uint32_t)size, buf);
                prints(buf);
                prints("\n");
                break;
            }
            break;
        }

        void *memlimit = syscall_memlimit(0);
        prints("Current memlimit is ");
        itoa((int)memlimit, buf);
        prints(buf);
        prints("\n");

        if(ptr != 0) {
            free(ptr);
            memlimit = syscall_memlimit(0);
            prints("Dallocated first block, memlimit is ");
            itoa((int)memlimit, buf);
            prints(buf);
            prints("\n");
        }
        size *= 2;
    }
    return 0;
}

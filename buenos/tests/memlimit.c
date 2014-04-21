#include "lib.h"

int main(void) {
    void *memlimit = syscall_memlimit(0);
    char buf[10];
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    prints("Requesting 1 bytes more memory\n");
    memlimit = syscall_memlimit(memlimit + 1);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    prints("Requesting 12052 bytes more memory\n");
    memlimit = syscall_memlimit(memlimit + 12052);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    *((char*)memlimit) = 'a';

    prints("Requesting 6000 bytes less memory\n");
    memlimit = syscall_memlimit(memlimit - 6000);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    prints("Requesting 6052 bytes less memory\n");
    memlimit = syscall_memlimit(memlimit - 6052);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    prints("Requesting 1 bytes less memory\n");
    memlimit = syscall_memlimit(memlimit - 1);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");

    return 0;
}

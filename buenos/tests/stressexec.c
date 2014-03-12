#include "tests/lib.h"

int main(void) {
    char* str = "balalaikka";
    char* str2 = "ripkukkoparssinen";
    char* ptrs[2];
    ptrs[0] = str;
    ptrs[1] = str2;
    char argc = 2;
    char retvalbuf[52];
    int i = 0;

    while(1) {
        int pid = syscall_execp("[testi]argprint", argc, (const char**)ptrs);
        int retval = syscall_join(pid);
        prints("return value ");
        itoa(retval, retvalbuf); 
        prints(retvalbuf);
        prints("\n");
        itoa(i, retvalbuf); 
        prints(retvalbuf);
        prints("\n");
        i++;
    }
    prints("exited ?!?!?!\n");
    return 5;
}

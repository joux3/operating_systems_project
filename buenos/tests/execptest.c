#include "tests/lib.h"

int main(void) {
    char* str = "balalaikka";
    char* str2 = "ripkukkoparssinen";
    char* ptrs[2];
    ptrs[0] = str;
    ptrs[1] = str2;
    char argc = 2;
    
    syscall_write(stdout, "starting execp test\n", 512);
    int s = syscall_execp("[testi]argcat", argc, (const char**)ptrs);
    return s;
}

#include "tests/lib.h"

int main(void) {
    char argc = 2;
    
    prints("starting execp test with illegal argv\n");
    int s = syscall_execp("[testi]argcat", argc, (const char**)0x8274);
    return s;
}

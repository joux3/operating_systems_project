#include "tests/lib.h"

int main(void) {
    char* str = "balalaikka";
    char argc = 1;
    
    int s = syscall_execp("[testi]argcat", argc, (const char**)&str);
    return s;
}

#include "tests/lib.h"

int main(void) {
    char argc = 1;
    char* argv = "useless";
    char filename[1024];
    uint32_t i = 0;
    for(i = 0; i < sizeof(filename); i++) {
        filename[i] = 'a';
    }
    
    prints("starting execp test with too long filename\n");
    int s = syscall_execp(filename, argc, (const char**)&argv);
    return s;
}

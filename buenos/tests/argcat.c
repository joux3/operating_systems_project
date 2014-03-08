#include "tests/lib.h"

int filehandle;
int read;

int main(int argc, char **argv) {
    syscall_write(stdout, "starting", 512);
    char buf[64];
    itoa((int)(argv[0][0]), buf);
    int result = argc;
    prints(buf);
    prints("\nwritten\n");


    return result;
}

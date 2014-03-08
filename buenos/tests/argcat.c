#include "tests/lib.h"

int filehandle;
int read;

int main(int argc, char **argv) {
    syscall_write(stdout, "starting", 512);
    char buf[64];
    itoa(argc, buf);
    int result = argc;
    prints(buf);
    prints((char*)argv[0]);
        prints("\nwritten\n");


    return result;
}

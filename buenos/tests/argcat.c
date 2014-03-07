#include "tests/lib.h"

int filehandle;
int read;

int main(int argc, char** argv) {
    int result = argc;
        syscall_write(stdout, (char*)argv, 512);
        prints("\nwritten\n");


    return result;
}

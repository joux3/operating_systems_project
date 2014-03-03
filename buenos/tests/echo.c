// program that writes out every read character
#include "tests/lib.h"

char buffer[2049];
int main(void) {
    int written;
    while(1) {
        int read = syscall_read(stdin, buffer, 2048);
        buffer[read++] = '\n';
        written = 0;
        while (written < read) {
            written += syscall_write(stdout, buffer + written, read - written);
        }
    }
    return 0;
}

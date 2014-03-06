#include "tests/lib.h"

char buffer[512];
int main(void) {
    int read, i, oldread;
    read = 0;
    while(1) {
        if (read == 512) {
            read--;
        }
        oldread = read;
        read += syscall_read(stdin, buffer + read, 512 - read);
        syscall_write(stdout, buffer + oldread, read - oldread);
        for (i = 0; i < read; i++) {
            if (buffer[i] == 13) {  
                buffer[i] = '\0';
                read = 0;
                syscall_exec(buffer);
            }
        }
    }
    return 0;
}

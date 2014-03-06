#include "tests/lib.h"

char buffer[512];
int main(void) {
    int read, i, oldread;
    char retval_buf[64];
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
                prints("\n");
                int pid = syscall_exec(buffer);
                if (pid < 0) {
                    prints("failed to start process: "); 
                    itoa(pid, retval_buf);
                    prints(retval_buf);
                    prints("\n");
                } else {
                    // TODO: parse & if we want to run in the background
                    int retval = syscall_join(pid);
                    prints("return value: "); 
                    itoa(retval, retval_buf);
                    prints(retval_buf);
                    prints("\n");
                }
            }
        }
    }
    return 0;
}

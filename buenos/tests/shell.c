#include "tests/lib.h"

char buffer[512];
int main(void) {
    int read, i, deletedchars;
    char retval_buf[64];
    int startbg;
    read = 0;
    prints("> ");
    while(1) {
        startbg = 0;
        if (read == 512) {
            read--;
        }
        read += syscall_read(stdin, buffer + read, 512 - read);
         
        deletedchars = 0;
        for (i = 0; i < read; i++) {
            if (buffer[i] == 127) {
                int j;
                for (j = i + 1; j < read; j++) {
                    buffer[j - 2] = buffer[j];
                }
                read -= 2;
                deletedchars++;
            }
        }
        read = (read < 0) ? 0 : read;
        
        prints("\r> ");
        syscall_write(stdout, buffer, read);
        for (i = 0; i < deletedchars; i++) {
            prints(" ");
        }
        prints("\r> ");
        syscall_write(stdout, buffer, read);
        for (i = 0; i < read; i++) {
            if (buffer[i] == '&') {
                startbg = 1;
            } else if (buffer[i] == 13) {  
                int j;
                buffer[i] = '\0';
                read = 0;
                prints("\n");
                // TODO: parse parameters properly
                for (j = 0; j < i; j++) {
                    if (buffer[j] == ' ') {
                        buffer[j] = 0;
                    }
                }
                int pid = syscall_exec(buffer);
                if (pid < 0) {
                    prints("failed to start process: "); 
                    itoa(pid, retval_buf);
                    prints(retval_buf);
                    prints("\n> ");
                } else {
                    if (!startbg) {
                        int retval = syscall_join(pid);
                        prints("return value: "); 
                        itoa(retval, retval_buf);
                        prints(retval_buf);
                        prints("\n> ");
                    }
                }
                break;
            }
        }
    }
    return 0;
}

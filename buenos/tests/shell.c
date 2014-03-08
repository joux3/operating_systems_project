#include "tests/lib.h"

char linebuffer[512];
char readbuffer[5];
int linelength; 
char retval_buf[64];

void run_linebuffer() {
    int i;
    int startbg = 0;

    for (i = 0; i < linelength; i++) {
        if (linebuffer[i] == '&') {
            startbg = 1;
        } 
    }

    int j;
    linebuffer[linelength++] = '\0';
    prints("\n");
    // TODO: parse parameters properly
    for (j = 0; j < i; j++) {
        if (linebuffer[j] == ' ') {
            linebuffer[j] = 0;
        }
    }

    linelength = 0;

    int pid = syscall_exec(linebuffer);
    if (pid < 0) {
        prints("failed to start process: "); 
        itoa(pid, retval_buf);
        prints(retval_buf);
    } else {
        if (!startbg) {
            int retval = syscall_join(pid);
            prints("return value: "); 
            itoa(retval, retval_buf);
            prints(retval_buf);
        } else {
            prints("started process "); 
            itoa(pid, retval_buf);
            prints(retval_buf);
            prints(" in background"); 
        }
    }
}

int main(void) {
    int i;
    linelength = 0;
    prints("> ");
    while(1) {
        int runcommand = 0;
        int deletedchars = 0;
        int read = syscall_read(stdin, readbuffer, 5);
         
        deletedchars = 0;
        for (i = 0; i < read; i++) {
            if (readbuffer[i] == 127) {
                if (linelength > 0) {
                    linelength--;
                    deletedchars++;
                }
            } else if (readbuffer[i] == 13) {
                runcommand = 1;
                break;
            } else {
                if (linelength < 511) {
                    linebuffer[linelength++] = readbuffer[i];
                }
            }
        }
        
        if (runcommand) {
            run_linebuffer();
            prints("\n> ");
        } else {
            prints("\r> ");
            syscall_write(stdout, linebuffer, linelength);
            for (i = 0; i < deletedchars; i++) {
                prints(" ");
            }
            prints("\r> ");
            syscall_write(stdout, linebuffer, linelength);
        }
    }
    return 0;
}

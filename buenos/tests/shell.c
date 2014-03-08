#include "tests/lib.h"

char linebuffer[512];
char readbuffer[5];
int linelength; 
char retval_buf[64];

char *argv[64];

void run_linebuffer() {
    int i;
    int startbg = 0;
    int argc = 1;

    argv[0] = linebuffer;

    for (i = 0; i < linelength; i++) {
        if (linebuffer[i] == '&') {
            startbg = 1;
        } 
    }

    linebuffer[linelength++] = '\0';
    prints("\n");

    for (i = 1; i < linelength; i++) {
        if (linebuffer[i] == ' ') {
            linebuffer[i] = '\0';
        } else if (linebuffer[i - 1] == '\0' && argc < 64) {
            argv[argc++] = &linebuffer[i];
        }
    }

    linelength = 0;
    /*
    prints("argv:\n");
    for (i = 0; i < argc; i++) {
        prints(argv[i]);
        prints("\n");
    }*/

    int pid = syscall_execp(linebuffer, argc, (const char**)argv);
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
                // backspace
                if (linelength > 0) {
                    linelength--;
                    deletedchars++;
                }
            } else if (readbuffer[i] == 13) {
                // enter
                runcommand = 1;
                break;
            } else {
                if (linelength < 511 && (linelength != 0 || readbuffer[i] != ' ')) {
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

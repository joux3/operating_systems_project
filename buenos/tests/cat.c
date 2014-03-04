#include "tests/lib.h"

int filehandle;
int read;
char buffer[512];

int main(void) {
    filehandle = syscall_open("[testi]labbalabba");
    if (filehandle > 0) {
        while((read = syscall_read(filehandle, buffer, 512)) > 0) {
            syscall_write(stdout, buffer, 512);
        }
        syscall_write(stdout, "\nread done\n", 11);
    } else {
        syscall_write(stdout, "failed to open file\n", 20);
    }
    filehandle = syscall_close(filehandle);
    if (!filehandle)
        syscall_write(stdout, "succeeded in closing file\n", 26);
    else
        syscall_write(stdout, "failed at closing file\n", 23);
    return 0;
}

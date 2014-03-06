#include "tests/lib.h"

int filehandle;
int read;
char buffer[512];

int main(void) {
    int result = 0;
    filehandle = syscall_open("[testi]labbalabba");
    if (filehandle > 0) {
        while((read = syscall_read(filehandle, buffer, 512)) > 0) {
            syscall_write(stdout, buffer, 512);
        }
        prints("\nread done, retrying with seek\n");
        syscall_seek(filehandle, 0);
        while((read = syscall_read(filehandle, buffer, 512)) > 0) {
            syscall_write(stdout, buffer, 512);
        }
        prints("\nread done\n");

      filehandle = syscall_close(filehandle);
      if (!filehandle)
          prints("succeeded in closing file\n");
      else
          prints("failed at closing file\n");
    } else {
        result = 715517;
        prints("failed to open file\n");
    }

    return result;
}

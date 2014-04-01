#include "tests/lib.h"

#define BUFSIZE 512

int filehandle;
int read;
char buffer[BUFSIZE];

int main(int argc, char **argv) {
    int result = 0;

    if (argc < 3) {
        prints("Usage: readtest <filename> <n>\n");
        return 1;
    }
    int n = atoi(argv[2]);

    int total = 0;
    int to_read = n - total  > BUFSIZE ? BUFSIZE : n - total;

    filehandle = syscall_open(argv[1]);
    if (filehandle > 0) {
        while(total < n && (read = syscall_read(filehandle, buffer, to_read)) > 0) {
            syscall_write(stdout, buffer, read);
            total += read;
            to_read = n - total  > BUFSIZE ? BUFSIZE : n - total;
        }
        prints("\nread with ");
        prints(argv[2]);
        prints("bytes done\n");

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

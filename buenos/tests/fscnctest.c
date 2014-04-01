#include "tests/lib.h"

#define MAX_BLOCK_SIZE 1024

#define WRITE_TEST "[testi]writetest"
#define READ_TEST "[testi]readtest"

// simple test that writes filesize bytes to filename
// in blocksize blocks
// compares read blocks to written
char char_for_pos(int pos) {
    return 'a' + (pos % ('z' - 'a'));
}

char buffer[MAX_BLOCK_SIZE];

int main(int argc, char **argv) {
    if (argc < 5) {
        prints("Usage: fscnctest  <filename> <filesize> <blocksize> <n_proc> \n");
        return 1;
    } 


    prints("Starting x amount of concurrent processes\n");


    int i, pid;
    int n_proc = atoi(argv[4]);
    int filesize = atoi(argv[2]);
    char retval_buf[12];
    for(i = 0; i < n_proc ; i++)
    {
        if(0) {
            argv[0] = WRITE_TEST;
            pid = syscall_execp(WRITE_TEST, argc - 1, (const char**)argv );
        }
        else {
            argv[0] = READ_TEST;
            char buf[24];
            itoa(filesize, buf);
            argv[2] = buf;
            prints(argv[2]); 
            pid = syscall_execp(READ_TEST, 3, (const char**)argv );
            filesize /= 4;
        }
        if (pid < 0) {
            prints("failed to start process: "); 
            itoa(pid, retval_buf);
            prints(retval_buf);
        } else {
                prints("started process "); 
                itoa(pid, retval_buf);
                prints(retval_buf);
                prints(" in background"); 
        }
    }
    return 0;
}

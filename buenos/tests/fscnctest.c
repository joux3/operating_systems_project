#include "tests/lib.h"

#define MAX_BLOCK_SIZE 1024
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


    int i;
    int n_proc = atoi(argv[4]);
    char* test_executable = "[testi]fstest";
    argv[0] = test_executable;
    char retval_buf[12];
    for(i = 0; i < n_proc ; i++)
    {
        int pid = syscall_execp(test_executable, argc - 1, (const char**)argv );
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

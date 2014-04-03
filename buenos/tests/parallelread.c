#include "tests/lib.h"

#define MAX_BLOCK_SIZE 1024

#define WRITE_TEST "[testi]writetest"
#define READ_TEST "[testi]readtest"
#define SMALL_FILESIZE "1"
#define BIG_FILESIZE "1024"

// simple test that writes filesize bytes to filename
// in blocksize blocks
// compares read blocks to written
char char_for_pos(int pos) {
    return 'a' + (pos % ('z' - 'a'));
}

char buffer[MAX_BLOCK_SIZE];

int main(int argc, char **argv) {
    if (argc < 5) {
        prints("Usage: fscnctest  <filename> <filesize> <n_big> <n_small> \n");
        return 1;
    } 
    char *filename = argv[1];
    int filesize = atoi(argv[2]);
    
    
    if (syscall_create(filename, filesize) < 0) {
        prints("failed to create file!\n");
    }
   

    prints("Starting x amount of concurrent processes\n");


    int pid;
    char retval_buf[12];
    int n_big = atoi(argv[3]);
    int n_small = atoi(argv[4]);
    
    argv[0] = READ_TEST;
    int i;
    for(i = 0; i < n_big; i++)
    {
        pid = syscall_execp(READ_TEST, 3, (const char**)argv );
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
    //argv[2] = SMALL_FILESIZE;
    for(i = 0; i < n_small; i++)
    {
        pid = syscall_execp(WRITE_TEST, 3, (const char**)argv );
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
    // all the reads opened before this point should go through
    pid = syscall_execp("[testi]rm", 2 , (const char**)argv);
        if (pid < 0) {
            prints("failed to start process: "); 
            itoa(pid, retval_buf);
            prints(retval_buf);
        } 
    return 0;
}

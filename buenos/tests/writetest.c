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
    if (argc < 3) {
        prints("Usage: fstest <filename> <n>\n");
        return 1;
    } 

    int blocksize = MAX_BLOCK_SIZE;

    char *filename = argv[1];
    int filesize = atoi(argv[2]);
    if (blocksize > MAX_BLOCK_SIZE) {
        prints("Blocksize must be smaller!\n");
        return 1;
    }

    
    int filehandle = syscall_open(filename);
    if (filehandle < 0) {
        prints("failed to open file!\n");
        return 3;
    }
    
    int written = 0;
    while(written < filesize) {
        int i;
        int write = MIN(filesize - written, blocksize);
        for (i = 0; i < write; i++) {
            buffer[i] = char_for_pos(written + i);
        }
        write = syscall_write(filehandle, (void*)&buffer, write);
        if (write <= 0) {
            prints("failed to write!\n");
            return 3;
        }
        written += write;
    }
    prints("OK, fstest wrote bytes. closing file\n");
    if(syscall_close(filehandle) == -1) {
        prints("OK, fstest file close failed\n");
        return 1;
    }
    
    prints("OK, fstest closed file\n");

    return 0;
}

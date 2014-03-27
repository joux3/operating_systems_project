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
    if (argc < 4) {
        prints("Usage: fstest <filename> <filesize> <blocksize>\n");
        return 1;
    } 

    char *filename = argv[1];
    int filesize = atoi(argv[2]);
    int blocksize = atoi(argv[3]);
    if (blocksize > MAX_BLOCK_SIZE) {
        prints("Blocksize must be smaller!\n");
        return 1;
    }

    if (syscall_create(filename, filesize) < 0) {
        prints("failed to create file!\n");
        return 2;
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
    prints("OK, fstest wrote bytes. checking result\n");
    syscall_seek(filehandle, 0);
    int total_read = 0;
    while(total_read < filesize) {
        int i;
        int read = MIN(filesize - total_read, blocksize);
        read = syscall_read(filehandle, (void*)&buffer, read);
        if (read <= 0) {
            prints("failed to read!\n");
            return 3;
        }
        for (i = 0; i < read; i++) {
            if (buffer[i] != char_for_pos(total_read + i)) {
                prints("READ CONTENTS DON'T MATCH THE WRITTEN CONTENTS! FS BUG!\n");
                return 4;
            }
        }
        total_read += read;
    }
    prints("read contents match the written contests!\n");

    return 0;
}

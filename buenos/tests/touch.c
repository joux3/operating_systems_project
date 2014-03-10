#include "tests/lib.h"

int main(int argc, char **argv) {
    int filehandle;
    int contentlen = 0;

    if (argc < 2) {
        prints("Usage: touch <filename> [content to write]\n");
        return 1;
    } 

    if (argc > 2) {
        int i;
        for (i = 2; i < argc; i++) {
            if (i > 2)
                contentlen += 1;
            
            contentlen += strlen(argv[i]); 
        }
    }

    if (syscall_create(argv[1], contentlen) < 0) {
        prints("failed to create file!\n");
        return 2;
    }

    if (argc > 2) {
        filehandle = syscall_open(argv[1]);
        if (filehandle >= 0) {
            int i;
            for (i = 2; i < argc; i++) {
                int written = 0;
                int len = strlen(argv[i]);
                if (i > 2) {
                    syscall_write(filehandle, " ", 1);
                }
                while (written < len) {
                    written += syscall_write(filehandle, (void*)(argv[i] + written), len - written);
                } 
            }
            syscall_close(filehandle);
        } else {
            prints("failed to open file for writing\n");
        } 
    }
    
    return 0;
}

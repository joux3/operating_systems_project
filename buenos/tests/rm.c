#include "tests/lib.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        prints("Usage: rm <filename>\n");
        return 1;
    } 
    
    if(syscall_delete(argv[1]) < 0) {
        prints("failed to delete file\n"); 
        return 2;
    } else {
        prints("deleted file\n"); 
        return 0;
    }
}

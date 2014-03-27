#include "tests/lib.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        prints("Usage: touchsize <filename> <size>\n");
        return 1;
    } 

    if (syscall_create(argv[1], atoi(argv[2])) < 0) {
        prints("failed to create file!\n");
        return 2;
    }

    return 0;
}

#include "tests/lib.h"


int main(int argc, char **argv) {
    int i;
    char buffer[52];
    prints("got argc:\n");
    itoa(argc, buffer);
    prints(buffer);
    prints("\n");
    prints("got argv:\n");
    itoa((int)argv, buffer);
    prints(buffer);
    prints("\n");
    prints("starting, got params:\n");
    for (i = 0; i < argc; i++) {
        prints(argv[i]);
        prints("\n");
    }
    prints("----- EOF\n");

    return 2;
}

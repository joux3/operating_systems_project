#include "lib.h"
char block[40000];

int main(void)
{
    int i = 0;
    for (i = 0; i < 40000; i++) {
        block[i] = 'a' + (i % 3);
    }
    while(1) {
        block[i] = 'a' + (i % 3);
        i = (i + 1) % 40000; 
    
        int probe = (i + 20000) % 40000;
        if (block[probe] != 'a' + (probe % 3)) 
            prints("MEMORY MISMATCH!!");
    }
    return 0;
}

// program that writes out every read character
#include "tests/lib.h"

char buffer[2049];
int main(int argc, char **argv) {
    if (argc > 1) {
            int i;
            for (i = 1; i < argc; i++) {
                int written = 0;
                int len = strlen(argv[i]);
		if(i > 1)
		{
                    syscall_write(stdout, " " , 1 );
		}
                while (written < len) {
                    written += syscall_write(stdout, (void*)(argv[i] + written), len - written);
                } 
            }
	    syscall_write(stdout, "\n" , 1 );
    }
    return 0;
}

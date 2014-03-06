#include "tests/lib.h"

int main(void) {
    int filehandle;
    int written;
    int len;
    char *content = "hehee läbbäläbbä. eihän sinne tarvita passia";
    len = 0;
    written = 0;
    while (content[len] != '\0') {
        len++;
    }
    syscall_create("[testi]labbalabba", 4000); 
    filehandle = syscall_open("[testi]labbalabba");
    if (filehandle >= 0) {
        while (written < len) {
            written += syscall_write(filehandle, (void*)&(content[written]), len - written);
        } 
        syscall_close(filehandle);
    }    
    
    return 0;
}

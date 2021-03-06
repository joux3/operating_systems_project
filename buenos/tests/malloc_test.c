#include "lib.h"


void *print_malloc(uint32_t size) {
    char buf[24];
	void *ptr = malloc(size);
    prints("Allocated, ptr is ");
    itoa((uint32_t)ptr, buf);
    prints(buf);
    prints("\n");

    void *memlimit = syscall_memlimit(0);
    prints("Current memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");
	return ptr;
}

void print_free(void *ptr) {
    char buf[24];
    free(ptr);
    void *memlimit = syscall_memlimit(0);
    prints("Dallocated first block, memlimit is ");
    itoa((int)memlimit, buf);
    prints(buf);
    prints("\n");
}

int main(void) {
    prints("start test\n");
    void* first_ptr;
    void* ptr_table[10];
    ptr_table[0] = print_malloc(2000);
    first_ptr = ptr_table[0];
    ptr_table[1] = print_malloc(100);
    ptr_table[2] = print_malloc(500);
    ptr_table[3] = print_malloc(200);

    
    print_free(ptr_table[1]);
    prints("this should divide a new deleted block");
    ptr_table[1] = print_malloc(30);
    
    prints("free first three, should merge into one \n");
    print_free(ptr_table[2]);
    print_free(ptr_table[1]);
    print_free(ptr_table[0]);
    
    prints("this should fit in the first allocated block now merged \n");
    ptr_table[0] = print_malloc(2616);
    if(ptr_table[0] != first_ptr)
        return 1;

    print_free(ptr_table[3]);
    print_free(ptr_table[0]);
    

    return 0;
}

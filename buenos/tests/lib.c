/*
 * Userland library functions
 *
 * Copyright (C) 2003 Juha Aatrokoski, Timo Lilja,
 *   Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: lib.c,v 1.6 2004/01/14 10:08:50 ttakanen Exp $
 *
 */

/* You probably want to add new functions to this file. To maintain
 * binary compatibility with other Buenoses (as probably required by
 * your assignments) DO NOT CHANGE EXISTING SYSCALL FUNCTIONS!
 */

#include "proc/syscall.h"
#include "tests/lib.h"

/* Halt the system (sync disks and power off). This function will
 * never return. 
 */
void syscall_halt(void)
{
    _syscall(SYSCALL_HALT, 0, 0, 0);
}


/* Load the file indicated by 'filename' as a new process and execute
 * it. Returns the process ID of the created process. Negative values
 * are errors.
 */
int syscall_exec(const char *filename)
{
    return (int)_syscall(SYSCALL_EXEC, (uint32_t)filename, 0, 0);
}

/* Load the file indicated by 'filename' as a new process and execute
 * it. Returns the process ID of the created process. Negative values
 * are errors.
 */

int syscall_execp(const char *filename, int argc, const char **argv)
{
    return (int)_syscall(SYSCALL_EXEC, (uint32_t)filename, 
                         (uint32_t) argc, 
                         (uint32_t) argv);
}


/* Exit the current process with exit code 'retval'. Note that
 * 'retval' must be non-negative since syscall_join's negative return
 * values are interpreted as errors in the join call itself. This
 * function will never return.
 */
void syscall_exit(int retval)
{
    _syscall(SYSCALL_EXIT, (uint32_t)retval, 0, 0);
}


/* Wait until the execution of the process identified by 'pid' is
 * finished. Returns the exit code of the joined process, or a
 * negative value on error.
 */
int syscall_join(int pid)
{
    return (int)_syscall(SYSCALL_JOIN, (uint32_t)pid, 0, 0);
}


/* Create a new thread running in the same address space as the
 * caller. The thread is started at function 'func', and the thread
 * will end when 'func' returns. 'arg' is passed as an argument to
 * 'func'. Returns 0 on success or a negative value on error.
 */
int syscall_fork(void (*func)(int), int arg)
{
    return (int)_syscall(SYSCALL_FORK, (uint32_t)func, (uint32_t)arg, 0);
}


/* (De)allocate memory by trying to set the heap to end at the address
 * 'heap_end'. Returns the new end address of the heap, or NULL on
 * error. If 'heap_end' is NULL, the current heap end is returned.
 */
void *syscall_memlimit(void *heap_end)
{
    return (void*)_syscall(SYSCALL_MEMLIMIT, (uint32_t)heap_end, 0, 0);
}


/* Open the file identified by 'filename' for reading and
 * writing. Returns the file handle of the opened file (positive
 * value), or a negative value on error.
 */
int syscall_open(const char *filename)
{
    return (int)_syscall(SYSCALL_OPEN, (uint32_t)filename, 0, 0);
}


/* Close the open file identified by 'filehandle'. Zero will be returned
 * success, other values indicate errors. 
 */
int syscall_close(int filehandle)
{
    return (int)_syscall(SYSCALL_CLOSE, (uint32_t)filehandle, 0, 0);
}


/* Read 'length' bytes from the open file identified by 'filehandle'
 * into 'buffer', starting at the current file position. Returns the
 * number of bytes actually read (e.g. 0 if the file position is at
 * the end of file) or a negative value on error.
 */
int syscall_read(int filehandle, void *buffer, int length)
{
    return (int)_syscall(SYSCALL_READ, (uint32_t)filehandle,
                    (uint32_t)buffer, (uint32_t)length);
}


/* Set the file position of the open file identified by 'filehandle'
 * to 'offset'. Returns 0 on success or a negative value on error. 
 */
int syscall_seek(int filehandle, int offset)
{
    return (int)_syscall(SYSCALL_SEEK,
			 (uint32_t)filehandle, (uint32_t)offset, 0);
}


/* Write 'length' bytes from 'buffer' to the open file identified by
 * 'filehandle', starting from the current file position. Returns the
 * number of bytes actually written or a negative value on error.
 */
int syscall_write(int filehandle, const void *buffer, int length)
{
    return (int)_syscall(SYSCALL_WRITE, (uint32_t)filehandle, (uint32_t)buffer,
                    (uint32_t)length);
}


/* Create a file with the name 'filename' and initial size of
 * 'size'. Returns 0 on success and a negative value on error. 
 */
int syscall_create(const char *filename, int size)
{
    return (int)_syscall(SYSCALL_CREATE, (uint32_t)filename, (uint32_t)size, 0);
}


/* Remove the file identified by 'filename' from the file system it
 * resides on. Returns 0 on success or a negative value on error. 
 */
int syscall_delete(const char *filename)
{
    return (int)_syscall(SYSCALL_DELETE, (uint32_t)filename, 0, 0);
}


void prints(const char *str) {
    int written;
    int len; 
    len = strlen(str);
    written = 0;
    while (written < len) {
        written += syscall_write(stdout, (void*)&(str[written]), len - written);
    }
}

int strlen(const char *str) {
    int len;
    len = 0;
    while(str[len] != '\0') {
        len++;
    }
    return len;
}

void itoa(int num, char *buf) {
    if (num < 0) {
        *(buf++) = '-';
        num *= -1;
    }

    if (num == 0) {
        *(buf++) = '0';
    } else {
        int div = 1;
        while ((num / div) > 0) {
            div *= 10;
        }
        div /= 10;
        while (div >= 1) {
            int d = num / div;
            *(buf++) = '0' + d;
            num -= d * div;
            div /= 10;
        }
    }    

    *buf = '\0';
}

int myisdigit(const char c) {
    return c >= '0' && c <= '9';
}

int atoi( const char *c ) {
    int value = 0;
    int sign = 1;
    if( *c == '+' || *c == '-' ) {
       if( *c == '-' ) sign = -1;
       c++;
    }
    while ( myisdigit( *c ) ) {
        value *= 10;
        value += (int) (*c-'0');
        c++;
    }
    return value * sign;
}

/**
 * Compares two strings. The strings must be null-terminated. If the
 * strings are equal returns 0. If the first string is greater than
 * the second one, returns a positive value. If the first string is
 * less than the second one, returns a negative value. This function
 * works like the strncpy function in the C library.
 *
 * @param str1 First string to compare.
 *
 * @param str2 The second string to compare.
 *
 * @return The difference of the first pair of bytes in str1 and str2
 * that differ. If the strings are equal returns 0.
 */
int stringcmp(const char *str1, const char *str2)
{
    while(1) {
        if (*str1 == '\0' && *str2 == '\0')
            return 0;
        if (*str1 == '\0' || *str2 == '\0' ||
            *str1 != *str2)
            return *str1-*str2;

        str1++;
        str2++;
    }

    /* Dummy return to keep gcc happy */
    return 0; 
}

#define MIN_FRAGMENT 24

uint32_t heap_bottom = 0;

typedef struct {
    uint32_t size;
    uint32_t is_deleted;
} alloc_header_t;

void *malloc(uint32_t size) {
    uint32_t  new_memlimit;
	uint32_t heap_end;
    alloc_header_t *cur_header;

	static uint32_t heap_bottom = 0;
	if(heap_bottom == 0)
		heap_bottom = (uint32_t)syscall_memlimit(0);

	heap_end = (uint32_t)syscall_memlimit(0);
	//ceil the size to next word bouyndary
	size = 3&size ? (~3&size)+4 : size;
    cur_header = (alloc_header_t*)heap_bottom;
    //check if free holes big enough in the already allocated area
    while(heap_end > heap_bottom && cur_header->size + (uint32_t)cur_header + sizeof(alloc_header_t) < heap_end) {
        //if fits here
        if(cur_header->is_deleted && (cur_header->size >= size)) {
            //if there is space to divide the end of the space to a new block 
            if(cur_header->size > size + sizeof(alloc_header_t) + MIN_FRAGMENT) {
                alloc_header_t *new_block_header = (alloc_header_t*)((uint32_t)cur_header + size + sizeof(alloc_header_t));
                new_block_header->is_deleted = 0;
                new_block_header->size = cur_header->size - size - sizeof(alloc_header_t); 
               //this only here because otherwise we keep the allocated size same
                cur_header->size = size;
            }
            cur_header->is_deleted = 0;
            return (void*)((uint32_t)cur_header + sizeof(alloc_header_t));
        }
        cur_header += cur_header->size + sizeof(alloc_header_t);
    }
    //couldnt find big enough hole so we have to move the program break
    // query current memlimit
    cur_header = (alloc_header_t*)heap_end;	

  // caluclate new limit with ceil to word boundary
    new_memlimit = (uint32_t)cur_header + sizeof(alloc_header_t) + size;

    if(syscall_memlimit((void*)new_memlimit) != (void*)new_memlimit)
        return 0;

    cur_header->size = size;
    cur_header->is_deleted = 0;
   
    return (void*)((uint32_t)cur_header + sizeof(alloc_header_t));
}

void free(void *ptr) {
	uint32_t heap_end;
    alloc_header_t *cur_header;
    alloc_header_t *bottom_header;

	//query heap end
	heap_end = (uint32_t)syscall_memlimit(0);
    //mark deleted
    cur_header = (alloc_header_t*)((uint32_t)ptr - sizeof(alloc_header_t));
    cur_header->is_deleted = 1;
    bottom_header = cur_header;
    //start merging deleted areas from this point onwards
    while(cur_header->is_deleted > 0) {
		if(bottom_header != cur_header)
			bottom_header->size += cur_header->size + sizeof(alloc_header_t); 
        cur_header = (alloc_header_t*)((uint32_t)cur_header + cur_header->size + sizeof(alloc_header_t));
        if((uint32_t)cur_header >= heap_end - sizeof(alloc_header_t)) {
            //lower the program break beacause blocks freed from the end of heap
            syscall_memlimit((void*)bottom_header);
            break;
        }
    }
    
}

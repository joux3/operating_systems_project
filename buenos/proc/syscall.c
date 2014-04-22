/*
 * System calls.
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
 * $Id: syscall.c,v 1.3 2004/01/13 11:10:05 ttakanen Exp $
 *
 */
#include "kernel/cswitch.h"
#include "proc/syscall.h"
#include "kernel/halt.h"
#include "kernel/panic.h"
#include "lib/libc.h"
#include "kernel/assert.h"
#ifdef CHANGED_2
    #include "fs/vfs.h"
    #include "lib/debug.h"
    #include "drivers/tty.h"
    #include "drivers/device.h"
    #include "vm/vm.h"
    #include "vm/pagepool.h"

    
    #define KERNEL_BUFFER_SIZE 256
#ifdef CHANGED_3
    #define IO_KERNEL_BUFFER_SIZE 1024
#else
#error
#endif

gcd_t *syscall_get_console_gcd(void) {
    // get first device (index 0) of type console
    return (gcd_t*)device_get(YAMS_TYPECODE_TTY, 0)->generic_device;
}

/**
 * Syscall handler functions
 */

void syscall_exit_process(int retval) {
    int i;
    process_id_t current_process;
    pagetable_t *pagetable;

    current_process = thread_get_current_process();

    DEBUG("processdebug", "process %d exit\n", current_process);

    lock_acquire(process_filehandle_lock);
    for (i = 0; i < CONFIG_MAX_OPEN_FILES; i++) {
        if (process_filehandle_table[i].in_use &&
            process_filehandle_table[i].owner == current_process) {
            vfs_close(process_filehandle_table[i].vfs_handle);
            process_filehandle_table[i].in_use = 0;
        }
    }
    lock_release(process_filehandle_lock);

    lock_acquire(process_table_lock);
    
    process_table[current_process].retval = retval;
    process_table[current_process].state = PROCESS_ZOMBIE;

    // clean the child processes of this process
    for (i = 0; i < CONFIG_MAX_PROCESS_COUNT; i++) {    
        if (process_table[i].state == PROCESS_ZOMBIE && process_table[i].parent == current_process) {
            process_table[i].state = PROCESS_FREE;
        }
    }

    lock_release(process_table_lock);

    condition_broadcast(process_zombie_cv);

    pagetable = thread_get_current_thread_entry()->pagetable;
    if (pagetable) {
        for (i = 0; i < (int)pagetable->valid_count; i++) {
            #ifdef CHANGED_4
            pagetable_entry_t *entry = &pagetable->entries[i];
            if (entry->even_page >= 0) {
                vm_free_virtual_page(entry->even_page); 
            }
            if (entry->odd_page >= 0) {
                vm_free_virtual_page(entry->odd_page); 
            }
            #else
            #error
            #endif
        }
    }
    vm_destroy_pagetable(pagetable);
    thread_get_current_thread_entry()->pagetable = NULL;
    
    thread_finish();

    KERNEL_PANIC("Shouldn't reach here...");
}


int open_file(char* filename) {
    int process_filehandle, i, status;
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    openfile_t openfile;

    status = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    if (status == 0) {
        return -1;
    } else if (status < 0) {
        syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
    }

    lock_acquire(process_filehandle_lock);

    process_filehandle = -1;
    for (i = 0; i < CONFIG_MAX_OPEN_FILES; i++) {
        if (!process_filehandle_table[i].in_use) {
            process_filehandle = i;
            break;
        }
    } 
    if (process_filehandle >= 0) {
        openfile = vfs_open(kernel_buffer);
        if (openfile >= 0) {
            process_filehandle_table[i].in_use = 1;
            process_filehandle_table[i].vfs_handle = openfile;
            process_filehandle_table[i].owner = thread_get_current_process();
        } else {
            process_filehandle = -1;
        }
    } 
    if (process_filehandle >= 0) {
        process_filehandle += 3;
    }
    
    lock_release(process_filehandle_lock);
    return process_filehandle;
}

int close_file(int filehandle) {
    int result;
    lock_acquire(process_filehandle_lock);
    filehandle -= 3;
    if (filehandle < 0 || filehandle >= CONFIG_MAX_OPEN_FILES) { 
        result = -1;
    } else if (!process_filehandle_table[filehandle].in_use ||
        process_filehandle_table[filehandle].owner != thread_get_current_process()) {
        result = -1;
    } else {
        result = vfs_close(process_filehandle_table[filehandle].vfs_handle);
        // Lazy deletion
        process_filehandle_table[filehandle].in_use = 0;
    }
    lock_release(process_filehandle_lock);
    return result;
}

int seek_file(int filehandle, int pos) {
    lock_acquire(process_filehandle_lock);
    filehandle -= 3;
    #ifdef CHANGED_3
    if (filehandle < 0 || filehandle >= CONFIG_MAX_OPEN_FILES) { 
        lock_release(process_filehandle_lock);
        return -1;
    } else if (!process_filehandle_table[filehandle].in_use ||
        process_filehandle_table[filehandle].owner != thread_get_current_process()) {
        lock_release(process_filehandle_lock);
        return -1;
    } else {
        process_filehandle_t *handle_entry = &process_filehandle_table[filehandle];
        lock_release(process_filehandle_lock);
        return vfs_seek(handle_entry->vfs_handle, pos);
    }
    #else
    #error
    #endif
}

int read_from_handle(int filehandle, void* buffer, int length) {
    #ifdef CHANGED_3
    int result, n;
    uint8_t kernel_buffer[IO_KERNEL_BUFFER_SIZE];
    gcd_t *console;

    length = length < IO_KERNEL_BUFFER_SIZE ? length : IO_KERNEL_BUFFER_SIZE;

    if (filehandle == FILEHANDLE_STDOUT || filehandle == FILEHANDLE_STDERR) {
        result = -1;
    } else if (filehandle == FILEHANDLE_STDIN) {
        console = syscall_get_console_gcd();
        result = console->read(console, kernel_buffer, length);
    } else if ((filehandle - 3) >= 0 && (filehandle - 3) < CONFIG_MAX_OPEN_FILES) { 
        lock_acquire(process_filehandle_lock);
        process_filehandle_t *handle_entry = &process_filehandle_table[filehandle - 3];
        if (handle_entry->in_use && handle_entry->owner == thread_get_current_process()) {
            lock_release(process_filehandle_lock);
            result = vfs_read(handle_entry->vfs_handle, kernel_buffer, length);
        } else {
            lock_release(process_filehandle_lock);
            result = -1;
        }
    } else {
        result = -1;
    }
    #else
    #error
    #endif

    if (result > 0) {
        n = kernel_to_userland_memcpy(kernel_buffer, buffer, result);
        if (n != result)
        {
            syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
        }
    }
    return result;
}

int write_to_handle(int filehandle, void* buffer, int length) {
    #ifdef CHANGED_3
    int result, n;
    uint8_t kernel_buffer[IO_KERNEL_BUFFER_SIZE];
    gcd_t *console;

    n = userland_to_kernel_memcpy(buffer, kernel_buffer, length < IO_KERNEL_BUFFER_SIZE ? length : IO_KERNEL_BUFFER_SIZE);
    if (n < 0)
    {
        syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
    }
        
    if (filehandle == FILEHANDLE_STDIN) {
        result = -1;
    } else if (filehandle == FILEHANDLE_STDOUT || filehandle == FILEHANDLE_STDERR) {
        console = syscall_get_console_gcd();
        result = console->write(console, kernel_buffer, n);
    } else if ((filehandle - 3) >= 0 && (filehandle - 3) < CONFIG_MAX_OPEN_FILES) {
        lock_acquire(process_filehandle_lock);
        process_filehandle_t *handle_entry = &process_filehandle_table[filehandle - 3];
        if (handle_entry->in_use && handle_entry->owner == thread_get_current_process()) {
            lock_release(process_filehandle_lock);
            result = vfs_write(handle_entry->vfs_handle, kernel_buffer, n);
        } else {
            lock_release(process_filehandle_lock);
            result = -1;
        }
    } else {
        result = -1;
    }
    #else
    #error
    #endif
    return result;
}

int create_file(char* filename, int size) {
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    int status;

    status = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    DEBUG("kernel_memory", "copy status %d\n", status);
    if (status == 0) {
        return -1;
    } else if (status < 0) {
        syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
    }
    return vfs_create(kernel_buffer, size);
}


int remove_file(char* filename) {
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    int status;

    status = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    DEBUG("kernel_memory", "copy status %d\n", status);
    if (status == 0) {
        return -1;
    } else if (status < 0) {
        syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
    }
    return vfs_remove(filename);
}

int execp_process(char *filename, char argc, char **argv) {
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    char arg_buffer[KERNEL_BUFFER_SIZE];
    int n_copied, i, n;
    DEBUG("processdebug", "%d arguments in execp\n", argc);

    n = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    DEBUG("processdebug", "copy status %d\n", n);
    if (n == 0){
        return -1;
    } else if (n < 0) {
        syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
    }

    if (argc >= (int)(KERNEL_BUFFER_SIZE / sizeof(void*))) {
        return -1;
    }

    // the same memory area is first used to hold pointers, then int offsets
    KERNEL_ASSERT(sizeof(void*) == sizeof(int));
    
    // first copy argv over
    n = userland_to_kernel_memcpy(argv, arg_buffer, argc * sizeof(void*));
    if (n == 0) {
        KERNEL_PANIC("should not happen as argc is already validated to be small enough\n");
    } else if (n < 0) {
        DEBUG("processdebug", "Illegal argv pointer\n", n);
        syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
    }
    
    n_copied = 0;
    // then copy strings in argv, calculate cumulative offsets 
    for (i = 0; i < argc; i++)
    {
        /*copy string to kernel buffer and leave argc amount of room to store the offsets  */
        DEBUG("processdebug", "trying to copy %d argument %s\n", i, ((char **)arg_buffer)[i]);
        n = userland_to_kernel_strcpy(((char **)arg_buffer)[i], arg_buffer + argc * sizeof(void*) + n_copied, sizeof(arg_buffer) - n_copied);
        *(int*)(arg_buffer + i * sizeof(void*)) = n_copied;
        DEBUG("processdebug", "copy status %d\n", n);
        DEBUG("processdebug", "%d offset is %d\n",i,  n_copied) ;
        if (n == 0) {
            return -1;
        } else if (n < 0) {
            DEBUG("processdebug", "Illegal argv string pointer\n", n);
            syscall_exit_process(SYSCALL_INVALID_USERLAND_POINTER);
        }
        /* count in the ending characters */
        n_copied += n + 1;
        
    }

    return process_start_args(kernel_buffer, arg_buffer, n_copied + argc * sizeof(int*), argc);
}


int process_join(int process) {
    int result;
    if (process < 0 || process > CONFIG_MAX_PROCESS_COUNT) {
        return -1;
    }
    lock_acquire(process_table_lock);
    if ((process_table[process].state == PROCESS_RUNNING || process_table[process].state == PROCESS_ZOMBIE) &&
        process_table[process].parent == thread_get_current_process()) {
        while(process_table[process].state == PROCESS_RUNNING) {
            condition_wait(process_zombie_cv, process_table_lock); 
        } 
        result = process_table[process].retval;
        process_table[process].state = PROCESS_FREE;
    } else {
        result = -2;
    }
    lock_release(process_table_lock);
    return result;
}

#endif

#ifdef CHANGED_4
void *memlimit(void *heap_end) 
{
    pagetable_t *pagetable = thread_get_current_thread_entry()->pagetable;
    if (!pagetable) 
        return NULL;

    uint32_t new_limit = (uint32_t)heap_end;
    
    // just return the current memlimit
    if (heap_end == NULL)
        return (void*)pagetable->memlimit;
        
    if (new_limit > pagetable->memlimit) {
        DEBUG("memlimit", "Moving memlimit up...\n");
        while((pagetable->memlimit & PAGE_SIZE_MASK) < (new_limit & PAGE_SIZE_MASK)) {
            if (pagetable->valid_count == PAGETABLE_ENTRIES) // pagetable full
                return NULL; // TODO: free the pages we successfully got before. that can be done by using the lowering memlimit code
            int new_page = vm_get_virtual_page();
            if (new_page < 0)
                return NULL; // TODO: free the pages we successfully got before. that can be done by using the lowering memlimit code
            pagetable->memlimit += PAGE_SIZE;
            vm_map(pagetable, new_page, pagetable->memlimit & PAGE_SIZE_MASK, 0);
            DEBUG("memlimit", " - mapped 0x%x -> virtual page %d\n", pagetable->memlimit & PAGE_SIZE_MASK, new_page);
        }
        // put it where the userland wanted it even if we actually allocate full pages
        pagetable->memlimit = new_limit;
        return (void*)new_limit;
    } 
    if (new_limit < pagetable->memlimit) {
        DEBUG("memlimit", "Moving memlimit down...\n");
        while((pagetable->memlimit & PAGE_SIZE_MASK) > (new_limit & PAGE_SIZE_MASK)) {
            DEBUG("memlimit", " - unmapping 0x%x\n", pagetable->memlimit & PAGE_SIZE_MASK);
            vm_unmap(pagetable, pagetable->memlimit & PAGE_SIZE_MASK);
            pagetable->memlimit -= PAGE_SIZE;
        }
        // put it where the userland wanted it even if we actually operate on pages
        pagetable->memlimit = new_limit;
        return (void*)new_limit;
    }
    return NULL;
}
#endif

/**
 * Handle system calls. Interrupts are enabled when this function is
 * called.
 *
 * @param user_context The userland context (CPU registers as they
 * where when system call instruction was called in userland)
 */
void syscall_handle(context_t *user_context)
{
    /* When a syscall is executed in userland, register a0 contains
     * the number of the syscall. Registers a1, a2 and a3 contain the
     * arguments of the syscall. The userland code expects that after
     * returning from the syscall instruction the return value of the
     * syscall is found in register v0. Before entering this function
     * the userland context has been saved to user_context and after
     * returning from this function the userland context will be
     * restored from user_context.
     */
    #ifdef CHANGED_2
        int result;
        result = -1;
    #endif

    switch(user_context->cpu_regs[MIPS_REGISTER_A0]) {
    case SYSCALL_HALT:
        halt_kernel();
        break;
    #ifdef CHANGED_2
        case SYSCALL_EXEC:
            DEBUG("processdebug", "execp called\n");
            result = execp_process((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]), (char)(user_context->cpu_regs[MIPS_REGISTER_A2]), (char**)(user_context->cpu_regs[MIPS_REGISTER_A3]));
            break;
        case SYSCALL_EXIT:
            syscall_exit_process((int)user_context->cpu_regs[MIPS_REGISTER_A1]);
            result = -1;
            break;
        case SYSCALL_JOIN:
            result = process_join((int)user_context->cpu_regs[MIPS_REGISTER_A1]);
            break;
        case SYSCALL_OPEN:
            result = open_file((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            break;
        case SYSCALL_CLOSE:
            result = close_file((int)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            break;
        case SYSCALL_SEEK:
            result = seek_file((int)(user_context->cpu_regs[MIPS_REGISTER_A1]),
                               (int)(user_context->cpu_regs[MIPS_REGISTER_A2]));
            break;
        case SYSCALL_READ:
            result = read_from_handle((int)(user_context->cpu_regs[MIPS_REGISTER_A1]),
                        (void*)(user_context->cpu_regs[MIPS_REGISTER_A2]),
                        (int)(user_context->cpu_regs[MIPS_REGISTER_A3]));
            break;
        case SYSCALL_WRITE:
            result = write_to_handle((int)(user_context->cpu_regs[MIPS_REGISTER_A1]),
                        (void*)(user_context->cpu_regs[MIPS_REGISTER_A2]),
                        (int)(user_context->cpu_regs[MIPS_REGISTER_A3]));
            break;
        case SYSCALL_CREATE:
            result = create_file((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]),
                        (int)(user_context->cpu_regs[MIPS_REGISTER_A2]));
            break;
        case SYSCALL_DELETE:
            result = remove_file((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            break;
    #endif
    #ifdef CHANGED_4
        case SYSCALL_MEMLIMIT:
            result = (int)memlimit((void*)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            break;
    #endif
    default: 
        KERNEL_PANIC("Unhandled system call\n");
    }

    /* Move to next instruction after system call */
    user_context->pc += 4;
    #ifdef CHANGED_2
        user_context->cpu_regs[MIPS_REGISTER_V0] = result;
    #endif
}

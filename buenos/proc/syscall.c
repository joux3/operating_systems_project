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

    #define KERNEL_BUFFER_SIZE 512

gcd_t *syscall_get_console_gcd(void) {
    // get first device (index 0) of type console
    return (gcd_t*)device_get(YAMS_TYPECODE_TTY, 0)->generic_device;
}

#endif

#ifdef CHANGED_2

/**
 * Syscall handler functions
 */

int open_file(char* filename) {
    int process_filehandle, i, status;
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    openfile_t openfile;

    status = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    if(status == 0){
        return -1;
    }
    else if(status < 0){
        KERNEL_PANIC("should call process exit\n");
    }

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
    return process_filehandle;
}

int close_file(int filehandle) {
    int result;
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
    return result;
}

int seek_file(int filehandle, int pos) {
    int result;
    filehandle -= 3;
    if (filehandle < 0 || filehandle >= CONFIG_MAX_OPEN_FILES) { 
        result = -1;
    } else if (!process_filehandle_table[filehandle].in_use ||
        process_filehandle_table[filehandle].owner != thread_get_current_process()) {
        result = -1;
    } else {
        result = vfs_seek(process_filehandle_table[filehandle].vfs_handle, pos);
    }
    return result;
}

int read_from_handle(int filehandle, void* buffer, int length) {
    int result, n;
    uint8_t kernel_buffer[KERNEL_BUFFER_SIZE];
    gcd_t *console;

    n = userland_to_kernel_memcpy(buffer, kernel_buffer, length < KERNEL_BUFFER_SIZE ? length : KERNEL_BUFFER_SIZE);
    if(n < 0)
    {
        KERNEL_PANIC("should call process exit\n");
    }

    if (filehandle == FILEHANDLE_STDOUT || filehandle == FILEHANDLE_STDERR) {
        result = -1;
    } else if (filehandle == FILEHANDLE_STDIN) {
        console = syscall_get_console_gcd();
        result = console->read(console, buffer, length);
    } else if ((filehandle - 3) >= 0 && (filehandle - 3) < CONFIG_MAX_OPEN_FILES) { 
        lock_acquire(process_filehandle_lock);
        process_filehandle_t *handle_entry = &process_filehandle_table[filehandle - 3];
        if (!handle_entry->in_use || handle_entry->owner == thread_get_current_process()) {
            result = vfs_read(handle_entry->vfs_handle, buffer, length);
        } else {
            result = -1;
        }
        lock_release(process_filehandle_lock);
    } else {
      result = -1;
    }
    return result;
}

int write_to_handle(int filehandle, void* buffer, int length) {
    int result, n;
    uint8_t kernel_buffer[KERNEL_BUFFER_SIZE];
    gcd_t *console;

    n = userland_to_kernel_memcpy(buffer, kernel_buffer, length < KERNEL_BUFFER_SIZE ? length : KERNEL_BUFFER_SIZE);
    if(n < 0)
    {
        KERNEL_PANIC("should call process exit\n");
    }
        
    if (filehandle == FILEHANDLE_STDIN) {
        result = -1;
    } else if (filehandle == FILEHANDLE_STDOUT || filehandle == FILEHANDLE_STDERR) {
        console = syscall_get_console_gcd();
        result = console->write(console, kernel_buffer, n);
    } else if ((filehandle - 3) >= 0 && (filehandle - 3) < CONFIG_MAX_OPEN_FILES) {
        // TODO find filehandle if a file
        lock_acquire(process_filehandle_lock);
        process_filehandle_t *handle_entry = &process_filehandle_table[filehandle - 3];
        if (!handle_entry->in_use || handle_entry->owner == thread_get_current_process()) {
            result = vfs_write(handle_entry->vfs_handle, kernel_buffer, n);
        } else {
            result = -1;
        }
        lock_release(process_filehandle_lock);
    } else {
        result = -1;
    }
    return result;
}

int create_file(char* filename, int size) {
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    int status;

    status = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    DEBUG("kernel_memory", "copy status %d\n", status);
    if(status == 0){
        return -1;
    }
    else if(status < 0){
        KERNEL_PANIC("should call process exit\n");
    }
    return vfs_create(kernel_buffer, size);
}


int remove_file(char* filename) {
    char kernel_buffer[KERNEL_BUFFER_SIZE];
    int status;

    status = userland_to_kernel_strcpy(filename, kernel_buffer, sizeof(kernel_buffer));
    DEBUG("kernel_memory", "copy status %d\n", status);
    if(status == 0){
        return -1;
    }
    else if(status < 0){
        KERNEL_PANIC("should call process exit\n");
    }
    return vfs_remove(filename);
}

int exec_process(char *filename) {
    // TODO: safely copy filename from userland to kernel
    return process_start(filename);
}

void exit_process(int retval) {
    int i;
    process_id_t current_process;
    pagetable_t *pagetable;

    current_process = thread_get_current_process();

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
            tlb_entry_t *entry = &pagetable->entries[i];
            if (entry->V0) {
                pagepool_free_phys_page(entry->PFN0 << 12); 
            }
            if (entry->V1) {
                pagepool_free_phys_page(entry->PFN1 << 12);
            }
        }
    }
    vm_destroy_pagetable(pagetable);
    thread_get_current_thread_entry()->pagetable = NULL;
    
    thread_finish();
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
    } else {
        result = -2;
    }
    lock_release(process_table_lock);
    return result;
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
            result = exec_process((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            break;
        case SYSCALL_EXIT:
            exit_process((int)user_context->cpu_regs[MIPS_REGISTER_A1]);
            result = -1;
            break;
        case SYSCALL_JOIN:
            result = process_join((int)user_context->cpu_regs[MIPS_REGISTER_A1]);
            break;
        case SYSCALL_OPEN:
            lock_acquire(process_filehandle_lock);
            result = open_file((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            lock_release(process_filehandle_lock);
            break;
        case SYSCALL_CLOSE:
            lock_acquire(process_filehandle_lock);
            result = close_file((int)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            lock_release(process_filehandle_lock);
            break;
        case SYSCALL_SEEK:
            lock_acquire(process_filehandle_lock);
            result = seek_file((int)(user_context->cpu_regs[MIPS_REGISTER_A1]),
                               (int)(user_context->cpu_regs[MIPS_REGISTER_A2]));
            lock_release(process_filehandle_lock);
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
    default: 
        KERNEL_PANIC("Unhandled system call\n");
    }

    /* Move to next instruction after system call */
    user_context->pc += 4;
    #ifdef CHANGED_2
        user_context->cpu_regs[MIPS_REGISTER_V0] = result;
    #endif
}

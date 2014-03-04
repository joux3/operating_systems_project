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

gcd_t *syscall_get_console_gcd(void) {
    // get first device (index 0) of type console
    return (gcd_t*)device_get(YAMS_TYPECODE_TTY, 0)->generic_device;
}

process_id_t syscall_get_current_process(void) {
    return thread_get_current_thread_entry()->process_id;
}

#endif

#ifdef CHANGED_2

/**
 * Syscall handler functions
 */

int open_file(char* filename) {
    // TODO safely copy filename to kernel
    int process_filehandle, i;
    openfile_t openfile;
    process_filehandle = -1;
    for (i = 0; i < CONFIG_MAX_OPEN_FILES; i++) {
        if (!process_filehandle_table[i].in_use) {
            process_filehandle = i;
            break;
        }
    } 
    if (process_filehandle >= 0) {
        openfile = vfs_open(filename);
        if (openfile >= 0) {
            process_filehandle_table[i].in_use = 1;
            process_filehandle_table[i].vfs_handle = openfile;
            process_filehandle_table[i].owner = syscall_get_current_process();
        } else {
            process_filehandle = -1;
        }
    } 
    if (process_filehandle >= 0) {
        process_filehandle += 3;
    }
    return process_filehandle;
}

int read_from_handle(int filehandle, void* buffer, int length) {
    // TODO: check that buffer is within user mapped region
    int result;
    gcd_t *console;
    if (filehandle == FILEHANDLE_STDOUT || filehandle == FILEHANDLE_STDERR) {
        result = -1;
    } else if (filehandle == FILEHANDLE_STDIN) {
        console = syscall_get_console_gcd();
        result = console->read(console, buffer, length);
    } else {
        lock_acquire(process_filehandle_lock);
        process_filehandle_t *handle_entry = &process_filehandle_table[filehandle - 3];
        if (!handle_entry->in_use || handle_entry->owner == syscall_get_current_process()) {
            result = vfs_read(handle_entry->vfs_handle, buffer, length);
        } else {
            result = -1;
        }
        lock_release(process_filehandle_lock);
    }
    return result;
    // TODO safely copy from kernel buffer to userland
}

int write_to_handle(int filehandle, void* buffer, int length) {
    int result;
    gcd_t *console;
    if (filehandle == FILEHANDLE_STDIN) {
        result = -1;
    } else if (filehandle == FILEHANDLE_STDOUT || filehandle == FILEHANDLE_STDERR) {
        // TODO find filehandle if a file
        // TODO safely copy to kernel
        console = syscall_get_console_gcd();
        result = console->write(console, buffer, length);
    } else {
        lock_acquire(process_filehandle_lock);
        result = -1;
        KERNEL_PANIC("Files not supported in syscall_write\n");
        lock_release(process_filehandle_lock);
    }
    return result;
}

int create_file(char* filename, int size) {
    // TODO safely copy filename from userland to kernel
    return vfs_create(filename, size);
}

int remove_file(char* filename) {
    // TODO safely copy filename from userland to kernel
    return vfs_remove(filename);

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
            KERNEL_PANIC("Unhandled system call\n");
            break;
        case SYSCALL_EXIT:
            KERNEL_PANIC("Unhandled system call\n");
            break;
        case SYSCALL_JOIN:
            KERNEL_PANIC("Unhandled system call\n");
            break;
        case SYSCALL_OPEN:
            lock_acquire(process_filehandle_lock);
            result = open_file((char*)(user_context->cpu_regs[MIPS_REGISTER_A1]));
            lock_release(process_filehandle_lock);
            break;
        case SYSCALL_CLOSE:
            KERNEL_PANIC("Unhandled system call\n");
            break;
        case SYSCALL_SEEK:
            KERNEL_PANIC("Unhandled system call\n");
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

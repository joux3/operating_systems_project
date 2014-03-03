/*
 * Process startup.
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
 * $Id: process.h,v 1.4 2003/05/16 10:13:55 ttakanen Exp $
 *
 */

#ifndef BUENOS_PROC_PROCESS
#define BUENOS_PROC_PROCESS

#ifdef CHANGED_2
    #include "kernel/lock_cond.h"
    #include "lib/types.h"
    #include "kernel/config.h"

    // TODO: figure out a way to skip this duplication
    typedef int openfile_t;
#endif

typedef int process_id_t;

#ifdef CHANGED_2

typedef enum {
    PROCESS_FREE,
    PROCESS_RUNNING,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct {
    char name[32];
    process_state_t state; 
    process_id_t parent;
    uint32_t retval;
} process_t;

process_t process_table[CONFIG_MAX_PROCESS_COUNT];
lock_t *process_table_lock;
cond_t *process_zombie_cv;

typedef struct {
    uint32_t free;
    process_id_t owner;
    openfile_t vfs_handle; 
} process_filehandle_t;

process_filehandle_t process_filehandle_table[CONFIG_MAX_OPEN_FILES];
lock_t *process_filehandle_lock;

#endif

void process_start(const char *executable);

#define USERLAND_STACK_TOP 0x7fffeffc

#endif

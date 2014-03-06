/*
 * Process startup.
 *
 * Copyright (C) 2003-2005 Juha Aatrokoski, Timo Lilja,
 *       Leena Salmela, Teemu Takanen, Aleksi Virtanen.
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
 * $Id: process.c,v 1.11 2007/03/07 18:12:00 ttakanen Exp $
 *
 */

#include "proc/process.h"
#include "proc/elf.h"
#include "kernel/thread.h"
#include "kernel/assert.h"
#include "kernel/interrupt.h"
#include "kernel/config.h"
#include "fs/vfs.h"
#include "drivers/yams.h"
#include "vm/vm.h"
#include "vm/pagepool.h"
#ifdef CHANGED_2
    #include "lib/debug.h"
#endif

#ifdef CHANGED_2
#include "lib/debug.h"
#endif 


/** @name Process startup
 *
 * This module contains a function to start a userland process.
 */

#ifdef CHANGED_2
extern thread_table_t thread_table[CONFIG_MAX_THREADS];

void process_init_process_table(void) {
    int i;

    process_table_lock = lock_create();
    process_filehandle_lock = lock_create();
    process_zombie_cv = condition_create();

    for (i = 0; i < CONFIG_MAX_PROCESS_COUNT; i++) {
        process_table[i].state = PROCESS_FREE;
    }
    
    for (i = 0; i < CONFIG_MAX_OPEN_FILES; i++) {
        process_filehandle_table[i].in_use = 0;
    }
}
#endif


/* TODO Set thread copy flags when creating thread */
#ifdef CHANGED_2
/*
void _kernel_to_userland_memcpy(void* mem, uint32_t lenmem, uint32_t* terminating_val)
{

}
*/
char* userland_to_kernel_strcpy(char* src, char* dst, uint32_t len)
{

    thread_table_t *my_entry;
    char * retval;
    uint32_t i;


    my_entry = thread_get_current_thread_entry();

    /* check that we are currently not on copy status */
    KERNEL_ASSERT(my_entry->on_kernel_copy == 0);


    my_entry->on_kernel_copy = 1;


    DEBUG("initprog", "starting strcpy\n");

    i = 0;
    retval = dst;
    while(i < len && src + i < (char*)USERLAND_STACK_TOP)
    {
        *(dst + i) = *(src + i);
        if(my_entry->copy_error_status != 0)
        {
            break;
        }
        if(*(dst + i) == 0)
        {
            goto exit;
        }
        i++;
    }
    retval = NULL;
    
    exit:

    my_entry->on_kernel_copy = 0;

    return retval;
}

/* returns NULL if failure, otherwise dst */
void* userland_to_kernel_memcpy(void* src, void* dst, uint32_t lenmem)
{

    thread_table_t *my_entry;
    void * retval;
    uint32_t i;

    my_entry = thread_get_current_thread_entry();

    /* check that we are currently not on copy status */
    KERNEL_ASSERT(my_entry->on_kernel_copy == 0);

    my_entry->on_kernel_copy = 1;

    for(i = 0; i < lenmem; i++)
    {
        /* if we are not on userland memory area return NULL */
        if(src + i >= (void*)USERLAND_STACK_TOP)
        {
            retval = NULL;
            goto exit;
        }
        *(uint8_t*)(dst + i) = *(uint8_t*)(src + i);
        /*check if exception flags rise */  
        if(my_entry->copy_error_status != 0)
        {
            retval = NULL;
            goto exit;
        }
    }
    
    retval = dst;
    
    exit:

    my_entry->on_kernel_copy = 0;

    return retval;
}
#endif

#ifdef CHANGED_2
void process_init(uint32_t entry_point) {
    context_t user_context;
    interrupt_status_t intr_status;
    thread_table_t *my_entry;

    DEBUG("processdebug", "in process_init, entrypoint %d\n", entry_point);

    my_entry = thread_get_current_thread_entry();

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = entry_point;

    thread_goto_userland(&user_context);

    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    KERNEL_PANIC("Thread returned from userland...");
}

int process_start(const char *executable)
{
    thread_table_t *new_entry;
    thread_table_t *my_entry;
    TID_t thread_id;
    process_id_t process_id;
    pagetable_t *pagetable;
    pagetable_t *original_pagetable;
    uint32_t phys_page;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;

    int i;
    
    interrupt_status_t intr_status;

    process_id = -1;
    my_entry = thread_get_current_thread_entry();

    original_pagetable = my_entry->pagetable;

    lock_acquire(process_table_lock);

    // TODO: find location for process from process table
    for (i = 0; i < CONFIG_MAX_PROCESS_COUNT; i++) {
        if (process_table[i].state == PROCESS_FREE) {
            process_id = i;
            break;
        }
    }
    if (process_id < 0) {
        lock_release(process_table_lock);
        return -1;
    }

    thread_id = thread_create(process_init, (uint32_t)elf.entry_point);
    new_entry = &thread_table[thread_id];
    new_entry->process_id = process_id;

    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    KERNEL_ASSERT(new_entry->pagetable == NULL);

    pagetable = vm_create_pagetable(thread_get_current_thread());
    KERNEL_ASSERT(pagetable != NULL);

    intr_status = _interrupt_disable();
    new_entry->pagetable = pagetable;
    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);
    /* Make sure the file existed and was a valid ELF file */
    KERNEL_ASSERT(file >= 0);
    KERNEL_ASSERT(elf_parse_header(&elf, file));

    /* Trivial and naive sanity check for entry point: */
    KERNEL_ASSERT(elf.entry_point >= PAGE_SIZE);

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    KERNEL_ASSERT(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
                  <= _tlb_get_maxindex() + 1);

    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(new_entry->pagetable, phys_page, 
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1);
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        DEBUG("processdebug", "mapping %x -> %x\n", elf.ro_vaddr + i*PAGE_SIZE, phys_page);
        vm_map(new_entry->pagetable, phys_page, 
               elf.ro_vaddr + i*PAGE_SIZE, 1);
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(new_entry->pagetable, phys_page, 
               elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    DEBUG("processdebug", "filling tlb for new process\n");

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(new_entry->pagetable);
    _interrupt_set_state(intr_status);
    
    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *)elf.ro_vaddr, 0, elf.ro_pages*PAGE_SIZE);
    memoryset((void *)elf.rw_vaddr, 0, elf.rw_pages*PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - 
        (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE;
    memoryset((void *)stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE*PAGE_SIZE);

    DEBUG("processdebug", "initialized new process memory to zero\n");

    /* Copy segments */

    if (elf.ro_size > 0) {
        DEBUG("processdebug", "copying read-only segment\n");
        /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.ro_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.ro_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.ro_vaddr, elf.ro_size)
              == (int)elf.ro_size);
        DEBUG("processdebug", "copied read-only segment\n");
    }


    if (elf.rw_size > 0) {
        /* Make sure that the segment is in proper place. */
        DEBUG("processdebug", "copying read-write segment\n");
        KERNEL_ASSERT(elf.rw_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.rw_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.rw_vaddr, elf.rw_size)
              == (int)elf.rw_size);
        DEBUG("processdebug", "copied read-write segment\n");
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        vm_set_dirty(new_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    // set asid for pagetable on new thread
    pagetable->ASID = thread_id;
    for (i = 0; i < (int)pagetable->valid_count; i++) {
        pagetable->entries[i].ASID = thread_id;
    }
    // manually overwrite the arg to point to entry point
    new_entry->context->cpu_regs[MIPS_REGISTER_A0] = elf.entry_point;
    thread_run(thread_id);
    
    // put the caller's TLB back

    intr_status = _interrupt_disable();
    my_entry->pagetable = original_pagetable;
    if (my_entry->pagetable) {
        tlb_fill(my_entry->pagetable);
    }
    _interrupt_set_state(intr_status);

    stringcopy(process_table[process_id].name, executable, 32);
    process_table[process_id].state = PROCESS_RUNNING;
    process_table[process_id].parent = thread_get_current_process();

    lock_release(process_table_lock);

    return process_id;
}
#else
/**
 * Starts one userland process. The thread calling this function will
 * be used to run the process and will therefore never return from
 * this function. This function asserts that no errors occur in
 * process startup (the executable file exists and is a valid ecoff
 * file, enough memory is available, file operations succeed...).
 * Therefore this function is not suitable to allow startup of
 * arbitrary processes.
 *
 * @executable The name of the executable to be run in the userland
 * process
 */
void process_start(const char *executable)
{
    thread_table_t *my_entry;
    pagetable_t *pagetable;
    uint32_t phys_page;
    context_t user_context;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;

    int i;

    interrupt_status_t intr_status;

    my_entry = thread_get_current_thread_entry();

    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    KERNEL_ASSERT(my_entry->pagetable == NULL);

    pagetable = vm_create_pagetable(thread_get_current_thread());
    KERNEL_ASSERT(pagetable != NULL);

    intr_status = _interrupt_disable();
    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);
    /* Make sure the file existed and was a valid ELF file */
    KERNEL_ASSERT(file >= 0);
    KERNEL_ASSERT(elf_parse_header(&elf, file));

    /* Trivial and naive sanity check for entry point: */
    KERNEL_ASSERT(elf.entry_point >= PAGE_SIZE);

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    KERNEL_ASSERT(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
                  <= _tlb_get_maxindex() + 1);

    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1);
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.ro_vaddr + i*PAGE_SIZE, 1);
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);
    
    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *)elf.ro_vaddr, 0, elf.ro_pages*PAGE_SIZE);
    memoryset((void *)elf.rw_vaddr, 0, elf.rw_pages*PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - 
        (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE;
    memoryset((void *)stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE*PAGE_SIZE);

    /* Copy segments */

    if (elf.ro_size > 0) {
    /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.ro_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.ro_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.ro_vaddr, elf.ro_size)
              == (int)elf.ro_size);
    }

    if (elf.rw_size > 0) {
    /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.rw_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.rw_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.rw_vaddr, elf.rw_size)
              == (int)elf.rw_size);
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        vm_set_dirty(my_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    /* Insert page mappings again to TLB to take read-only bits into use */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = elf.entry_point;

    thread_goto_userland(&user_context);

    KERNEL_PANIC("thread_goto_userland failed.");
}
#endif
/** @} */

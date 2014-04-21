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

/* 
    param direction > 0 if userland to kernel
    returns
        positive if successful
        0 if len was reached (ie. kernel buffer was too small)
        negative if an exception occured during copy (most likely invalid userland pointer)
*/
int kernel_strcpy(char* src, char* dst, uint32_t len, uint32_t direction)
{
    thread_table_t *my_entry;
    uint32_t i;
    char* userland_ptr;

    my_entry = thread_get_current_thread_entry();

    /* check that we are currently not on copy status */
    KERNEL_ASSERT(my_entry->on_kernel_copy == 0);


    my_entry->on_kernel_copy = 1;

    userland_ptr = direction ? src : dst;
    
    i = 0;
    while(i < len)
    {
        if(userland_ptr + i >= (char*)USERLAND_STACK_TOP)
        {
            my_entry->on_kernel_copy = 0;
            return -1;
        }
        *(dst + i) = *(src + i);
        if(my_entry->copy_error_status != 0)
        {
            my_entry->on_kernel_copy = 0;
            return -1;
        }
        if(*(dst + i) == 0)
        {
            my_entry->on_kernel_copy = 0;
            return i;
        }
        i++;
    }
    my_entry->on_kernel_copy = 0;
    return 0;
}

/* 
    param direction > 0 if userland to kernel
    returns
        copied length if successful
        negative if an exception occured during copy (most likely invalid userland pointer)
*/
int kernel_memcpy(void* src, void* dst, uint32_t lenmem, uint32_t direction)
{

    thread_table_t *my_entry;
    uint32_t i;
    void * userland_ptr;

    my_entry = thread_get_current_thread_entry();

    userland_ptr = direction ? src : dst;

    /* check that we are currently not on copy status */
    KERNEL_ASSERT(my_entry->on_kernel_copy == 0);

    my_entry->on_kernel_copy = 1;

    for(i = 0; i < lenmem; i++)
    {
        /* if we are not on userland memory area return negative */
        if(userland_ptr + i >= (void*)USERLAND_STACK_TOP)
        {
            my_entry->on_kernel_copy = 0;
            return -1;
        }
        *(uint8_t*)(dst + i) = *(uint8_t*)(src + i);
        /*check if exception flags rise */  
        if(my_entry->copy_error_status != 0)
        {
            my_entry->on_kernel_copy = 0;
            return -1;
        }
    }
    my_entry->on_kernel_copy = 0;
    return i;
}

int userland_to_kernel_strcpy(char* src, char* dst, uint32_t len)
{
    return kernel_strcpy(src, dst, len, 1);
}

int kernel_to_userland_strcpy(char* src, char* dst, uint32_t len)
{
    return kernel_strcpy(src, dst, len, 0);
}

int userland_to_kernel_memcpy(void* src, void* dst, uint32_t lenmem)
{
    return kernel_memcpy(src, dst, lenmem, 1);
}

int kernel_to_userland_memcpy(void* src, void* dst, uint32_t lenmem)
{
    return kernel_memcpy(src, dst, lenmem, 0);
}

void process_init(uint32_t entry_point) {
    context_t user_context;
    thread_table_t *my_entry;
    uint32_t argv;
    uint32_t argc;
    uint32_t i;

    DEBUG("processdebug", "in process_init, entrypoint %d\n", entry_point);

    my_entry = thread_get_current_thread_entry();
    
    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));

    argv = (my_entry->context->cpu_regs[MIPS_REGISTER_A1]);
    argc = (my_entry->context->cpu_regs[MIPS_REGISTER_A2]);

    DEBUG("processdebug", " n arguments %d\n", argc);
    DEBUG("processdebug", " stack ptr %X\n", argv);
    for(i = 0; i < argc; i++)
    {
        DEBUG("processdebug", " %u, 0x%x argument %s\n", i, (((char**)argv)[i]), (((char**)argv)[i]));

    }

    /* pull out the extra arguments */
    
    // reserve stack space for argument registers A0, A1
    user_context.cpu_regs[MIPS_REGISTER_SP] = (uint32_t)(argv - 2*sizeof(void*)); 
    user_context.cpu_regs[MIPS_REGISTER_A0] = (uint32_t)argc;
    user_context.cpu_regs[MIPS_REGISTER_A1] = (uint32_t)argv;

    DEBUG("processdebug", "set arguments\n", argv);

    user_context.pc = entry_point;

    thread_goto_userland(&user_context);

    KERNEL_PANIC("Thread returned from userland...");
}

int process_start(const char *executable)
{
    return process_start_args(executable, NULL, 0,0);
}

int process_start_args(const char *executable, void *arg_data, int arg_datalen, int arg_count )
{
    thread_table_t *new_entry;
    thread_table_t *my_entry;
    TID_t thread_id;
    process_id_t process_id;
    pagetable_t *pagetable;
    pagetable_t *original_pagetable;
    #ifdef CHANGED_4
    #else
    uint32_t phys_page;
    #endif
    uint32_t stack_bottom, stack_top;
    elf_info_t elf;
    openfile_t file;
    int invalid;

    int i, n;
 
    interrupt_status_t intr_status;

    process_id = -1;
    invalid = 0;
    my_entry = thread_get_current_thread_entry();

    original_pagetable = my_entry->pagetable;

    lock_acquire(process_table_lock);

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
    if (thread_id < 0) {
        lock_release(process_table_lock);
        return -2;
    }
  
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
    // set my pagetable too to support context switches during process start
    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);
    /* Make sure the file existed and was a valid ELF file */
    invalid = invalid || (file < 0);
    invalid = invalid || !elf_parse_header(&elf, file);
    /* Trivial and naive sanity check for entry point: */
    invalid = invalid || (elf.entry_point < PAGE_SIZE);
    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    invalid = invalid || (elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE > _tlb_get_maxindex() + 1);
    if (invalid) {
        intr_status = _interrupt_disable();
        my_entry->pagetable = original_pagetable;
        _interrupt_set_state(intr_status);

        new_entry->state = THREAD_FREE;

        #ifdef CHANGED_4
        tlb_clean();
        _tlb_set_asid(thread_get_current_thread());
        #else
        tlb_fill(my_entry->pagetable);
        #endif

        vm_destroy_pagetable(pagetable);

        lock_release(process_table_lock);
        return -1;
    }


    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        #ifdef CHANGED_4
        int virtual_page = vm_get_virtual_page();
        KERNEL_ASSERT(virtual_page != -1);
        vm_map(new_entry->pagetable, virtual_page, 
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE);
        #else
        #error
        #endif
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        #if CHANGED_4
        int virtual_page = vm_get_virtual_page();
        KERNEL_ASSERT(virtual_page != -1);
        DEBUG("processdebug", "mapping %x -> %x\n", elf.ro_vaddr + i*PAGE_SIZE, virtual_page);
        vm_map(new_entry->pagetable, virtual_page, 
               elf.ro_vaddr + i*PAGE_SIZE);
        #else
        #error
        #endif
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        // elf segments might have overlapped just enough to be on the same virtual page
        // so make sure we're not mapping them again
        int j;   
        int mapped = 0;
        for (j = 0; j < (int)new_entry->pagetable->valid_count; j++) {
            #ifdef CHANGED_4
            pagetable_entry_t *pagetable_entry = &new_entry->pagetable->entries[j];
            if (pagetable_entry->VPN == ((elf.rw_vaddr + i*PAGE_SIZE) >> 13)) {
                if (ADDR_IS_ON_EVEN_PAGE(elf.rw_vaddr + i*PAGE_SIZE)) {
                    mapped = pagetable_entry->even_page != -1;
                    break;
                } else {
                    mapped = pagetable_entry->odd_page != -1;
                    break;
                }
            }
            #else
            #error
            #endif
        }
        if (!mapped) {
            #ifdef CHANGED_4
            int virtual_page = vm_get_virtual_page();
            KERNEL_ASSERT(virtual_page != 0);
            DEBUG("processdebug", "mapping %x -> %x\n", elf.rw_vaddr + i*PAGE_SIZE, virtual_page);
            vm_map(new_entry->pagetable, virtual_page, 
                   elf.rw_vaddr + i*PAGE_SIZE);
            #else
            #error
            #endif
        } else {
            DEBUG("processdebug", "skipping already mapped %x\n", elf.rw_vaddr + i*PAGE_SIZE);
        }
    }

    DEBUG("processdebug", "filling tlb for new process\n");

    intr_status = _interrupt_disable();
    #ifdef CHANGED_4
    tlb_clean();
    _tlb_set_asid(thread_get_current_thread());
    #else
    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    tlb_fill(new_entry->pagetable);
    #endif
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
    // also write dirty bits for read-write pages as those might overlap ro pages
    for(i = 0; i < (int)elf.rw_pages; i++) {
        vm_set_dirty(new_entry->pagetable, elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    // manually overwrite the arg to point to entry point
    
    new_entry->context->cpu_regs[MIPS_REGISTER_A0] = elf.entry_point;

    /* copy the arguments to userland stack and set registers */
    
    /* set new stacktop on under arguments, align on word boundary */
    stack_top = ((USERLAND_STACK_TOP - arg_datalen) & (~3));

    void* ptr_kernel = arg_data;
    void* ptr_usrland = (void*)stack_top;
    
    DEBUG("processdebug", "stack top is now %X\n", stack_top);
    DEBUG("processdebug", "datalen is %d\n", arg_datalen);
    /* insert the pointers before the argument strings */
    /* modify the offsets to actual userland pointers */
    for(i = 0; i < arg_count; i++) {
        int offset = *(int*)ptr_kernel; 
        DEBUG("processdebug", "%d offset %d\n", i, offset);
        char* tmp_kernel_ptr = offset + (char*)stack_top + arg_count*sizeof(void*);
        DEBUG("processdebug", "%d pointer points %X\n", i, tmp_kernel_ptr);
        n = kernel_to_userland_memcpy(&tmp_kernel_ptr, ptr_usrland, sizeof(char*));
        /* has to be able to get the data so 0 copied is unacceptable */
        KERNEL_ASSERT(n > 0);
        ptr_usrland += sizeof(char*);
        ptr_kernel += sizeof(char*);
    }
    DEBUG("processdebug", "copied %d pointers\n", arg_count);
    DEBUG("processdebug", "usrland at %X kernel at %X\n", ptr_usrland, ptr_kernel);

    /* copy the strings after the pointers */
    n = kernel_to_userland_memcpy(ptr_kernel, ptr_usrland, arg_datalen - arg_count * sizeof(char*));
    DEBUG("processdebug", "copy status %d\n", n);
    /* exception in copying unacceptable. 0 copied is accepted since no arguments is accepted */
    KERNEL_ASSERT(n >= 0);
    DEBUG("processdebug", "added arguments on top of the stack\n");
    // force in some extra arguments 
    // stack ptr 
    new_entry->context->cpu_regs[MIPS_REGISTER_A1] = stack_top;
    // arg count
    new_entry->context->cpu_regs[MIPS_REGISTER_A2] = arg_count;
    DEBUG("processdebug", "run new thread\n");

    // set asid for pagetable on new thread
    pagetable->ASID = thread_id;
    #ifdef CHANGED_4
    #else
    for (i = 0; i < (int)pagetable->valid_count; i++) {
        pagetable->entries[i].ASID = thread_id;
    }
    #endif

    stringcopy(process_table[process_id].name, executable, 32);
    process_table[process_id].state = PROCESS_RUNNING;
    process_table[process_id].parent = thread_get_current_process();

    thread_run(thread_id);
    
    // put the caller's TLB back

    intr_status = _interrupt_disable();
    my_entry->pagetable = original_pagetable;
    if (my_entry->pagetable) {
        tlb_clean();
        _tlb_set_asid(thread_get_current_thread());
    }
    _interrupt_set_state(intr_status);

    lock_release(process_table_lock);

    vfs_close(file);

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

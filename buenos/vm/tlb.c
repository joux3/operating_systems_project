/*
 * TLB handling
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
 * $Id: tlb.c,v 1.6 2004/04/16 10:54:29 ttakanen Exp $
 *
 */

#include "kernel/panic.h"
#include "kernel/assert.h"
#include "vm/tlb.h"
#include "vm/pagetable.h"
#ifdef CHANGED_4
#include "lib/debug.h"
#include "lib/libc.h"
#include "kernel/thread.h"
#include "vm/vm.h"
#include "kernel/interrupt.h"

extern virtual_page_t *virtual_pool;
extern phys_page_t *phys_pool;

// fully clean the TLB. useful when there might be many invalid TLB entries
void tlb_clean(void)
{
    interrupt_status_t intr_status;
    intr_status = _interrupt_disable();

    DEBUG("tlbdebug", "TLB: cleaning tlb\n");
    tlb_entry_t zero_entry;
    memoryset(&zero_entry, 0, sizeof(tlb_entry_t));
    uint32_t i;
    for (i = 0; i <= _tlb_get_maxindex(); i++) {
        _tlb_write(&zero_entry, i, 1);
    }

    _interrupt_set_state(intr_status);
}

static void print_tlb_debug(tlb_exception_state_t *tes, char* name)
{
    DEBUG("tlbdebug", "TLB %s exception. Details:\n"
           "Failed Virtual Address: 0x%8.8x\n"
           "Virtual Page Number:    0x%8.8x\n"
           "ASID (Thread number):   %d\n",
           name, tes->badvaddr, tes->badvpn2, tes->asid);
}

// this writes the pagetable entry into the tlb. 
// it assumes that atleast one of the pages resides in physical memory
// if _tlb_probe finds a corresponding entry, it gets updated
// if not, it does write_random
// returns 1 if a previous entry was updated, 0 if new one was created
int upsert_into_tlb(pagetable_entry_t *entry, uint32_t ASID) {
    tlb_entry_t tlb_entry;
    memoryset(&tlb_entry, 0, sizeof(tlb_entry_t));
    tlb_entry.ASID = ASID;
    tlb_entry.VPN2 = entry->VPN;
    
    int tlb_loc = _tlb_probe(&tlb_entry);

    // construct the new tlb entry by checking if the virtual pages lie in memory
    if (entry->even_page >= 0 && virtual_pool[entry->even_page].phys_page >= 0) {
        tlb_entry.V0 = 1;
        phys_page_t *phys_page = &phys_pool[virtual_pool[entry->even_page].phys_page];
        tlb_entry.PFN0 = phys_page->phys_address >> 12;
        tlb_entry.D0 = phys_page->dirty;
    }
    if (entry->odd_page >= 0 && virtual_pool[entry->odd_page].phys_page >= 0) {
        tlb_entry.V1 = 1;
        phys_page_t *phys_page = &phys_pool[virtual_pool[entry->odd_page].phys_page];
        tlb_entry.PFN1 = phys_page->phys_address >> 12;
        tlb_entry.D1 = phys_page->dirty;
    }

    if (tlb_loc < 0) {
        _tlb_write_random(&tlb_entry);
        return 0;
    } else {
        KERNEL_ASSERT(_tlb_write(&tlb_entry, tlb_loc, 1) == 1);
        return 1;
    }
}

// returns 1 if handled properly
int tlb_modified_exception(void)
{
    tlb_exception_state_t tes;
    _tlb_get_exception_state(&tes);

    DEBUG("tlbdebug", "current thread %d\n", thread_get_current_thread());
    print_tlb_debug(&tes, "modified");

    thread_table_t *my_entry = thread_get_current_thread_entry();
    if (!my_entry->pagetable)
        return -1;

    pagetable_t *pagetable = my_entry->pagetable;

    uint32_t i;
    // go through the pagetable, find the virtual page and
    // mark it as dirty in the phys page table
    for (i = 0; i < pagetable->valid_count; i++) {
        pagetable_entry_t *entry = &pagetable->entries[i]; 
        DEBUG("tlbdebug", "entry vpn 0x%x, even %d, odd %d\n", entry->VPN, entry->even_page, entry->odd_page);
        if (entry->VPN == tes.badvpn2) {
            int virtual_page = -1;
            if (entry->even_page >= 0 && ADDR_IS_ON_EVEN_PAGE(tes.badvaddr)) {
                virtual_page = entry->even_page; 
            } else if (entry->odd_page >= 0 && ADDR_IS_ON_ODD_PAGE(tes.badvaddr)) {
                virtual_page = entry->odd_page; 
            }
            if (virtual_page != -1) {
                // mark the corresponding phys page as dirty
                vm_virtual_page_modified(virtual_page);
                // write the page as dirty to TLB
                KERNEL_ASSERT(upsert_into_tlb(entry, pagetable->ASID) == 1);
                return 1;
            }
        }
    }

    return -1;
}

// handles both kinds of tlb misses
// if is_store is set, then the dirty flag will be set to 1 in the phys page
int tlb_miss(int is_store)
{
    tlb_exception_state_t tes;
    _tlb_get_exception_state(&tes);

    print_tlb_debug(&tes, "miss");

    thread_table_t *my_entry = thread_get_current_thread_entry();
    if (!my_entry->pagetable)
        return -1;

    pagetable_t *pagetable = my_entry->pagetable;

    DEBUG("tlbdebug", "pagetable has %d entries\n", pagetable->valid_count);

    uint32_t i;
    for (i = 0; i < pagetable->valid_count; i++) {
        pagetable_entry_t *entry = &pagetable->entries[i]; 
        DEBUG("tlbdebug", "entry vpn 0x%x, even %d, odd %d\n", entry->VPN, entry->even_page, entry->odd_page);
        if (entry->VPN == tes.badvpn2) {
            if (entry->even_page >= 0 && ADDR_IS_ON_EVEN_PAGE(tes.badvaddr)) {
                vm_ensure_page_in_memory(entry->even_page, is_store); 
                upsert_into_tlb(entry, pagetable->ASID);
                return 1;
            } else if (entry->odd_page >= 0 && ADDR_IS_ON_ODD_PAGE(tes.badvaddr)) {
                vm_ensure_page_in_memory(entry->odd_page, is_store); 
                upsert_into_tlb(entry, pagetable->ASID);
                return 1;
            }
            break;
        }
    }

    return -1;
}

// returns 1 if handled properly
int tlb_load_exception(void)
{    
    return tlb_miss(0);
}

// returns 1 if handled properly
int tlb_store_exception(void)
{
    return tlb_miss(1);
}
#else
void tlb_modified_exception(void)
{
    KERNEL_PANIC("Unhandled TLB modified exception");
}

void tlb_load_exception(void)
{
    KERNEL_PANIC("Unhandled TLB load exception");
}

void tlb_store_exception(void)
{
    KERNEL_PANIC("Unhandled TLB store exception");
}
#endif


#ifdef CHANGED_4
#else
/**
 * Fill TLB with given pagetable. This function is used to set memory
 * mappings in CP0's TLB before we have a proper TLB handling system.
 * This approach limits the maximum mapping size to 128kB.
 *
 * @param pagetable Mappings to write to TLB.
 *
 */
void tlb_fill(pagetable_t *pagetable)
{
    if(pagetable == NULL)
	return;

    /* Check that the pagetable can fit into TLB. This is needed until
     we have proper VM system, because the whole pagetable must fit
     into TLB. */
    KERNEL_ASSERT(pagetable->valid_count <= (_tlb_get_maxindex()+1));

    _tlb_write(pagetable->entries, 0, pagetable->valid_count);

    /* Set ASID field in Co-Processor 0 to match thread ID so that
       only entries with the ASID of the current thread will match in
       the TLB hardware. */
    _tlb_set_asid(pagetable->ASID);
}
#endif

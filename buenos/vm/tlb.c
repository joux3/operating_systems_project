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

static void print_tlb_debug(tlb_exception_state_t *tes)
{
    DEBUG("tlbdebug", "TLB exception. Details:\n"
           "Failed Virtual Address: 0x%8.8x\n"
           "Virtual Page Number:    0x%8.8x\n"
           "ASID (Thread number):   %d\n",
           tes->badvaddr, tes->badvpn2, tes->asid);
}

// returns 1 if handled properly
int tlb_modified_exception(void)
{
    tlb_exception_state_t tes;
    _tlb_get_exception_state(&tes);

    DEBUG("tlbdebug", "current thread %d\n", thread_get_current_thread());
    print_tlb_debug(&tes);

    thread_table_t *my_entry = thread_get_current_thread_entry();
    if (!my_entry->pagetable)
        return -1;

    pagetable_t *pagetable = my_entry->pagetable;

    DEBUG("tlbdebug", "pagetable has %d entries\n", pagetable->valid_count);

    uint32_t i;
    for (i = 0; i < pagetable->valid_count; i++) {
        tlb_entry_t *entry = &pagetable->entries[i]; 
        DEBUG("tlbdebug", "entry vpn2 0x%x\n", entry->VPN2);
        if (entry->V0)
            DEBUG("tlbdebug", "   - PFN0 0x%x, , D0 %d\n", entry->PFN0, entry->D0);
        if (entry->V1)
            DEBUG("tlbdebug", "   - PFN1 0x%x, , D1 %d\n", entry->PFN1, entry->D1);
    }

    KERNEL_PANIC("Unhandled TLB modified exception");
    return -1;
}

// returns 1 if handled properly
int tlb_load_exception(void)
{    
    tlb_exception_state_t tes;
    _tlb_get_exception_state(&tes);

    print_tlb_debug(&tes);

    thread_table_t *my_entry = thread_get_current_thread_entry();
    if (!my_entry->pagetable)
        return -1;

    pagetable_t *pagetable = my_entry->pagetable;

    DEBUG("tlbdebug", "pagetable has %d entries\n", pagetable->valid_count);

    uint32_t i;
    for (i = 0; i < pagetable->valid_count; i++) {
        tlb_entry_t *entry = &pagetable->entries[i]; 
        DEBUG("tlbdebug", "entry vpn2 0x%x\n", entry->VPN2);
        if (entry->VPN2 == tes.badvpn2) {
            if (entry->V0 && ADDR_IS_ON_EVEN_PAGE(tes.badvaddr)) {
                _tlb_write_random(entry);
                return 1;
            } else if (entry->V1 && ADDR_IS_ON_ODD_PAGE(tes.badvaddr)) {
                _tlb_write_random(entry);
                return 1;
            }
            break;
        }
    }

    return -1;
}

// returns 1 if handled properly
int tlb_store_exception(void)
{
    tlb_exception_state_t tes;
    _tlb_get_exception_state(&tes);

    print_tlb_debug(&tes);

    thread_table_t *my_entry = thread_get_current_thread_entry();
    if (!my_entry->pagetable)
        return -1;

    pagetable_t *pagetable = my_entry->pagetable;

    DEBUG("tlbdebug", "pagetable has %d entries\n", pagetable->valid_count);

    uint32_t i;
    for (i = 0; i < pagetable->valid_count; i++) {
        tlb_entry_t *entry = &pagetable->entries[i]; 
        DEBUG("tlbdebug", "entry vpn2 0x%x\n", entry->VPN2);
        if (entry->VPN2 == tes.badvpn2) {
            if (entry->V0 && ADDR_IS_ON_EVEN_PAGE(tes.badvaddr)) {
                _tlb_write_random(entry);
                return 1;
            } else if (entry->V1 && ADDR_IS_ON_ODD_PAGE(tes.badvaddr)) {
                _tlb_write_random(entry);
                return 1;
            }
            break;
        }
    }

    return -1;
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
// Fill the TLB with the first pages from the pagetable
// As many as pages that fit into TLB are used
#else
/**
 * Fill TLB with given pagetable. This function is used to set memory
 * mappings in CP0's TLB before we have a proper TLB handling system.
 * This approach limits the maximum mapping size to 128kB.
 *
 * @param pagetable Mappings to write to TLB.
 *
 */
#endif

void tlb_fill(pagetable_t *pagetable)
{
    if(pagetable == NULL)
	return;

    #ifdef CHANGED_4
    // disable interrups to be sure that we can in one go fill and clean the TLB
    interrupt_status_t intr_status;
    intr_status = _interrupt_disable();
    #else
    /* Check that the pagetable can fit into TLB. This is needed until
     we have proper VM system, because the whole pagetable must fit
     into TLB. */
    KERNEL_ASSERT(pagetable->valid_count <= (_tlb_get_maxindex()+1));
    #endif

    #ifdef CHANGED_4
    uint32_t i;
    DEBUG("tlbdebug", "IN TLB FILL, ASID %d\n", pagetable->ASID);
    for (i = 0; i < pagetable->valid_count; i++) {
        tlb_entry_t *entry = &pagetable->entries[i]; 
        DEBUG("tlbdebug", "entry vpn2 0x%x\n", entry->VPN2);
        if (entry->V0)
            DEBUG("tlbdebug", "   - PFN0 0x%x, , D0 %d\n", entry->PFN0, entry->D0);
        if (entry->V1)
            DEBUG("tlbdebug", "   - PFN1 0x%x, , D1 %d\n", entry->PFN1, entry->D1);
    }
    _tlb_write(pagetable->entries, 0, MIN(pagetable->valid_count, _tlb_get_maxindex()+1));
    #else
    _tlb_write(pagetable->entries, 0, pagetable->valid_count);
    #endif

    #ifdef CHANGED_4
    // tlb_fill gets called in places, where something funky might be happening with the tlb:
    // - ownership of a pagetable might be moving between threads etc
    // so clean the rest of the TLB to avoid invalid entries
    tlb_entry_t zero_entry;
    memoryset(&zero_entry, 0, sizeof(tlb_entry_t));
    for (i = pagetable->valid_count; i <= _tlb_get_maxindex(); i++) {
        _tlb_write(&zero_entry, i, 1);
    }
    #endif

    /* Set ASID field in Co-Processor 0 to match thread ID so that
       only entries with the ASID of the current thread will match in
       the TLB hardware. */
    _tlb_set_asid(pagetable->ASID);

    #ifdef CHANGED_4
    _interrupt_set_state(intr_status);
    #endif
}

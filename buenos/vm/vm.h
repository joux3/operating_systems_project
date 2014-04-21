/*
 * Virtual memory initialization
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
 * $Id: vm.h,v 1.5 2003/05/21 10:42:29 lsalmela Exp $
 *
 */

#ifndef BUENOS_VM_VM_H
#define BUENOS_VM_VM_H

#include "vm/pagetable.h"

#ifdef CHANGED_2
/* Check whether given (virtual) address is even or odd mapping
   in a pair of mappings for TLB. */
#define ADDR_IS_ON_ODD_PAGE(addr)  ((addr) & 0x00001000)  
#define ADDR_IS_ON_EVEN_PAGE(addr) (!((addr) & 0x00001000))  
#else
#endif

#ifdef CHANGED_4

typedef struct virtual_page_struct_t {
    // flag that tells if this virtual page is in use or free
    uint8_t in_use;
    // index to phys_pool if this page is currently in memory. -1 otherwise
    int phys_page;
} virtual_page_t;

typedef struct phys_page_struct_t {
    // only set at init; the actual physical page this metadata belongs to
    uint32_t phys_address;
    // flag that tells if this physical page is in use or free
    uint8_t in_use;
    // the variables below are only useful if in_use is 1

    // the virtual page that is currently in this physical page
    uint32_t virtual_page;
    // last time when this page was participant in TLB exception
    uint32_t ticks;
    // a flag that tells if this page has had a TLB modification
    // exception since loading from disk
    // (i.e. do we need to write this page to disk when swapping it out?)
    uint8_t dirty;
} phys_page_t;

#endif

void vm_init(void);

pagetable_t *vm_create_pagetable(uint32_t asid);
void vm_destroy_pagetable(pagetable_t *pagetable);

#ifdef CHANGED_4
int vm_get_virtual_page();
void vm_free_virtual_page();
// TODO: we don't support write protected pages
void vm_map(pagetable_t *pagetable, int virtual_page, uint32_t vaddr);
#else
void vm_map(pagetable_t *pagetable, uint32_t physaddr, 
	    uint32_t vaddr, int dirty);
#endif
void vm_unmap(pagetable_t *pagetable, uint32_t vaddr);

void vm_set_dirty(pagetable_t *pagetable, uint32_t vaddr, int dirty);

#endif /* BUENOS_VM_VM_H */

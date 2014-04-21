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
 * $Id: vm.c,v 1.11 2004/01/13 11:09:50 ttakanen Exp $
 *
 */

#include "vm/pagetable.h"
#include "vm/vm.h"
#include "vm/pagepool.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#ifdef CHANGED_4
#include "drivers/device.h"
#include "drivers/gbd.h"
#include "lib/debug.h"
#include "kernel/interrupt.h"
#include "drivers/metadev.h"
#endif

/** @name Virtual memory system
 *
 * Functions for pagetable handling and page mappings.
 *
 * @{
 */

#ifdef CHANGED_2
#else
/* Check whether given (virtual) address is even or odd mapping
   in a pair of mappings for TLB. */
#define ADDR_IS_ON_ODD_PAGE(addr)  ((addr) & 0x00001000)  
#define ADDR_IS_ON_EVEN_PAGE(addr) (!((addr) & 0x00001000))  
#endif

#ifdef CHANGED_4
gbd_t *swap_gbd;
uint32_t virtual_pool_size;
virtual_page_t *virtual_pool;
uint32_t phys_pool_size;
phys_page_t *phys_pool;
#endif


/**
 * Initializes virtual memory system. Initialization consists of page
 * pool initialization and disabling static memory reservation. After
 * this kmalloc() may not be used anymore.
 */ 
void vm_init(void)
{
    /* Make sure that tlb_entry_t really is exactly 3 registers wide
       and thus probably also matches the hardware TLB registers. This
       is needed for assembler wrappers used for TLB manipulation. 
       Any extensions to pagetables should also provide this information
       in this form. */
    KERNEL_ASSERT(sizeof(tlb_entry_t) == 12);

    #ifdef CHANGED_4
    // find out the swap gbd by looking for disk with block size PAGE_SIZE
    int i = 0;
    while (1) {
        device_t *disk = device_get(YAMS_TYPECODE_DISK, i);
        if (disk == NULL)
            KERNEL_PANIC("No swap disk found!");
        swap_gbd = disk->generic_device;
        if (swap_gbd->block_size(swap_gbd) == PAGE_SIZE) {
            virtual_pool_size = swap_gbd->total_blocks(swap_gbd);
            kprintf("Pagepool: using the disk at 0x%x for swap, %d virtual pages\n", 
                    disk->io_address, virtual_pool_size);
            break;
        }
        i++;
    }

    // reserve enough memory for the virtual page entries 
    // (one virtual page per disk block)
    virtual_pool = (virtual_page_t*)kmalloc(virtual_pool_size * sizeof(virtual_page_t));
    if (!virtual_pool) 
        KERNEL_PANIC("Not enough memory left for virtual page pool data!");
    // statically reserve phys pool entries from the free memory left
    phys_pool_size = (kmalloc_get_numpages() - kmalloc_get_reserved_pages())/2; 
    KERNEL_ASSERT(phys_pool_size > 0);
    phys_pool = (phys_page_t*)kmalloc(phys_pool_size * sizeof(phys_page_t)); 
    if (!phys_pool)
        KERNEL_PANIC("Not enough memory left for physical page pool data!");
        
    #endif

    pagepool_init();
    kmalloc_disable();

    #ifdef CHANGED_4
    // dynamically reserve the physical pages from pagepool
    // (just for the page alignment, we don't really need the freeing)
    for (i = 0; (uint32_t)i < phys_pool_size; i++) {
        phys_pool[i].phys_address = pagepool_get_phys_page();
        phys_pool[i].state = PAGE_FREE;
        if (!phys_pool[i].phys_address)
            KERNEL_PANIC("Not enough memory left for physical pages!");
    }
    DEBUG("swapdebug", "SWAP: Allocated a total of %d physical pages for paging\n", phys_pool_size);

    KERNEL_ASSERT(sizeof(pagetable_t) <= PAGE_SIZE);
    #endif
}

#ifdef CHANGED_4

int swap_write_block(uint32_t block, uint32_t addr) {
    gbd_request_t req;

    req.block = block;
    req.sem = NULL;
    req.buf = ADDR_KERNEL_TO_PHYS(addr);
    return swap_gbd->write_block(swap_gbd, &req);
}

int swap_read_block(uint32_t block, uint32_t addr) {
    gbd_request_t req;

    req.block = block;
    req.sem = NULL;
    req.buf = ADDR_KERNEL_TO_PHYS(addr);
    return swap_gbd->read_block(swap_gbd, &req);
}

// swap the given physical page to disk
// this should be called with interrupts disabled
void swap_page(phys_page_t *phys_page) {
    KERNEL_ASSERT(phys_page->state == PAGE_IN_USE);
     
    phys_page->state = PAGE_UNDER_IO;
    if (phys_page->dirty) {
        // NOTE: there's a potential concurrency problem here, TODO
        // the process can still write the memory at phys_address
        KERNEL_ASSERT(swap_write_block(phys_page->virtual_page, phys_page->phys_address) != 0);
    }

    // stop the phys page from being used
    virtual_pool[phys_page->virtual_page].phys_page = -1;

    // at this point the old mapping for the physical page might be in TLB, so clean it
    // TODO: only remove the needed entry, not clean the whole table
    tlb_clean();

    phys_page->state = PAGE_FREE;
}

// finds a free virtual page and returns its id
// returns negative if no virtual pages left
int vm_get_virtual_page() 
{
    // disable to interrupts instead of locking
    interrupt_status_t intr_status;
    intr_status = _interrupt_disable();

    uint32_t i;
    for (i = 0; i < virtual_pool_size; i++) {
        if (!virtual_pool[i].in_use) {
            virtual_pool[i].in_use = 1;
            _interrupt_set_state(intr_status);
        
            virtual_pool[i].phys_page = -1;
            // clear out the space on disk (we don't keep track of whether virtual pages have
            // actually been in memory before or not)
            uint32_t buffer = pagepool_get_phys_page();
            if (!buffer) {
                virtual_pool[i].in_use = 0;
                return -1;
            }
            swap_write_block(i, buffer);
            pagepool_free_phys_page(buffer);

            DEBUG("swapdebug", "Reserved virtual page %d\n", i);
            return i;
        }
    }

    _interrupt_set_state(intr_status);
    return -1;
}

// free the given virtual page
void vm_free_virtual_page(int virtual_page)
{
    KERNEL_ASSERT(virtual_page >= 0 && virtual_page < (int)virtual_pool_size);

    // disable to interrupts instead of locking
    interrupt_status_t intr_status;
    intr_status = _interrupt_disable();

    KERNEL_ASSERT(virtual_pool[virtual_page].in_use);

    if (virtual_pool[virtual_page].phys_page >= 0) {
        int phys_page = virtual_pool[virtual_page].phys_page;
        KERNEL_ASSERT(phys_pool[phys_page].virtual_page == (uint32_t)virtual_page);
        phys_pool[phys_page].state = PAGE_FREE;
    }
    virtual_pool[virtual_page].in_use = 0;

    _interrupt_set_state(intr_status);
}

// sets the corresponding phys page of this virtual page as dirty
void vm_virtual_page_modified(int virtual_page)
{
    /* Interrupts _must_ be disabled when calling this function: */
    interrupt_status_t intr_state = _interrupt_get_state();
    KERNEL_ASSERT((intr_state & INTERRUPT_MASK_ALL) == 0 
                  || !(intr_state & INTERRUPT_MASK_MASTER));

    KERNEL_ASSERT(virtual_page >= 0 && virtual_page < (int)virtual_pool_size);
    
    virtual_page_t *page = &virtual_pool[virtual_page];
    KERNEL_ASSERT(page->phys_page >= 0);
    phys_page_t *phys_page = &phys_pool[page->phys_page];
    KERNEL_ASSERT(phys_page->state == PAGE_IN_USE);
    KERNEL_ASSERT(phys_page->dirty == 0);

    phys_page->dirty = 1;
    phys_page->ticks = rtc_get_msec();
}

// makes sure that the given virtual page is in memory
// also sets the dirty flag if wanted
void vm_ensure_page_in_memory(int virtual_page, int dirty)
{
    /* Interrupts _must_ be disabled when calling this function: */
    interrupt_status_t intr_state = _interrupt_get_state();
    KERNEL_ASSERT((intr_state & INTERRUPT_MASK_ALL) == 0 
                  || !(intr_state & INTERRUPT_MASK_MASTER));

    KERNEL_ASSERT(virtual_page >= 0 && virtual_page < (int)virtual_pool_size);
    
    virtual_page_t *page = &virtual_pool[virtual_page];
    KERNEL_ASSERT(page->in_use);
    phys_page_t *phys_page = NULL;
    if (page->phys_page >= 0) {
        // page is already in memory, OK
        phys_page = &phys_pool[page->phys_page];
    } else {
        // find a free phys page
        uint32_t i;
        uint32_t oldest_i = phys_pool_size;
        uint32_t oldest_ticks = 0;
        for (i = 0; i < phys_pool_size; i++) {
            if (phys_pool[i].state == PAGE_FREE) {
                DEBUG("swapdebug", "   - found free phys page %d\n", i);
                page->phys_page = i;
                phys_page = &phys_pool[i];
                break;
            } else if (phys_pool[i].state == PAGE_IN_USE && (oldest_i == phys_pool_size || phys_pool[i].ticks < oldest_ticks)) {
                // keep track of the oldest phys page in use
                oldest_i = i;
                oldest_ticks = phys_pool[i].ticks; 
            }
        }
        if (!phys_page) {
            // need to swap some page out. select the one with the oldest ticks
            if (oldest_i == phys_pool_size)
                KERNEL_PANIC("Need to swap out a page but none possible found!");
            DEBUG("swapdebug", "   - swapping out phys page %d\n", oldest_i);
            phys_page = &phys_pool[oldest_i];
            swap_page(phys_page);
            KERNEL_ASSERT(phys_page->state == PAGE_FREE);
            page->phys_page = oldest_i;
        }

        DEBUG("swapdebug", "   - swapping in page %d\n", virtual_page);
        phys_page->virtual_page = virtual_page;
        phys_page->state = PAGE_UNDER_IO;

        KERNEL_ASSERT(swap_read_block(virtual_page, phys_page->phys_address) != 0);

        phys_page->state = PAGE_IN_USE;
    }

    KERNEL_ASSERT(phys_page->state == PAGE_IN_USE);
    KERNEL_ASSERT((int)phys_page->virtual_page == virtual_page);

    phys_page->ticks = rtc_get_msec();
    phys_page->dirty = dirty;
}

#endif


/**
 *  Creates a new page table. Reserves memory (one page) for the table
 *  and sets the address space identifier for the created page table.
 *
 *  @param asid Address space identifier
 *
 *  @return The created page table
 *
 */

pagetable_t *vm_create_pagetable(uint32_t asid)
{
    pagetable_t *table;
    uint32_t addr;

    addr = pagepool_get_phys_page();
    if(addr == 0) {
        return NULL;
    }

    /* Convert physical page address to kernel unmapped
       segmented address. Since the size of that segment is 512MB,
       this way works only for pages allocated in the first 512MB of
       physical memory. */
    table = (pagetable_t *) (ADDR_PHYS_TO_KERNEL(addr));

    table->ASID        = asid;
    table->valid_count = 0;

    return table;
}

/**
 * Destroys given pagetable. Frees the memory (one page) allocated for
 * the pagetable. Does not remove mappings from the TLB.
 *
 * @param pagetable Page table to destroy
 *
 */

void vm_destroy_pagetable(pagetable_t *pagetable)
{
    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS((uint32_t) pagetable));
}

#if CHANGED_4

/**
 * Maps given virtual address to given physical address in given page
 * table. Does not modify TLB. The mapping is done in 4k chunks (pages).
 *
 * @param pagetable Page table in which to do the mapping
 *
 * @param vaddr Virtual address to map. This address should be in the
 * beginning of a page boundary (4k).
 *
 * @param virtual_page Virtual page to map to given virtual address.
 * This value should have come from vm_get_virtual_page()
 *
 */

void vm_map(pagetable_t *pagetable, 
            int virtual_page, 
            uint32_t vaddr)
{
    unsigned int i;

    if (virtual_page < 0 || virtual_page >= (int)virtual_pool_size)
        KERNEL_PANIC("Tried to map an unexistant virtual page!");

    for(i=0; i<pagetable->valid_count; i++) {
        if(pagetable->entries[i].VPN == (vaddr >> 13)) {
            /* TLB has separate mappings for even and odd 
               virtual pages. Let's handle them separately here,
               and we have much more fun when updating the TLB later.*/
            if(ADDR_IS_ON_EVEN_PAGE(vaddr)) {
                if(pagetable->entries[i].even_page >= 0) {
                    KERNEL_PANIC("Tried to re-map same virtual page");
                } else {
                    /* Map the page on a pair entry */
                    pagetable->entries[i].even_page = virtual_page;
                    return;
                }
            } else {
                if(pagetable->entries[i].odd_page >= 0) {
                    KERNEL_PANIC("Tried to re-map same virtual page");
                } else {
                    /* Map the page on a pair entry */
                    pagetable->entries[i].odd_page = virtual_page;
                    return;
                }
            }
        }
    }
    /* No previous or pairing mapping was found */

    /* Make sure that pagetable is not full */
    if(pagetable->valid_count >= PAGETABLE_ENTRIES) {
        kprintf("Thread with ASID=%d run out of pagetable mapping entries\n",
                pagetable->ASID);
        kprintf("during an attempt to map vaddr 0x%8.8x => virtual page %d\n",
                vaddr, virtual_page);
        KERNEL_PANIC("Thread run out of pagetable mapping entries.");
    }

    /* Map the page on a new entry */

    pagetable->entries[pagetable->valid_count].VPN = vaddr >> 13;

    if(ADDR_IS_ON_EVEN_PAGE(vaddr)) {
        pagetable->entries[pagetable->valid_count].even_page = virtual_page;
        pagetable->entries[pagetable->valid_count].odd_page = -1;
    } else {
        pagetable->entries[pagetable->valid_count].odd_page = virtual_page;
        pagetable->entries[pagetable->valid_count].even_page = -1;
    }

    pagetable->valid_count++;
}

#else
#error
#endif

/**
 * Unmaps given virtual address from given pagetable.
 *
 * @param pagetable Page table to operate on
 *
 * @param vaddr Virtual addres to unmap
 *
 */

void vm_unmap(pagetable_t *pagetable, uint32_t vaddr)
{
    pagetable = pagetable;
    vaddr     = vaddr;
    
    /* Not implemented */
}

/**
 * Sets the dirty bit for the given virtual page in the given
 * pagetable. The page must already be mapped in the pagetable.
 * If a page is marked dirty it can be read and written. If it is
 * clean (not dirty), it can be only read.
 *
 * @param pagetable The pagetable where the mapping resides.
 *
 * @param vaddr The virtual address whose dirty bit is to be set.
 *
 * @param dirty What the dirty bit is set to. Must be 0 or 1.
 */
void vm_set_dirty(pagetable_t *pagetable, uint32_t vaddr, int dirty)
{
    #ifdef CHANGED_4
    // NOP for now as we don't support write protected virtual pages
    pagetable = pagetable;
    vaddr = vaddr;
    dirty = dirty;
    #else
    unsigned int i;

    KERNEL_ASSERT(dirty == 0 || dirty == 1);

    for(i=0; i<pagetable->valid_count; i++) {
        if(pagetable->entries[i].VPN2 == (vaddr >> 13)) {
            /* Check whether this is an even or odd page */
            if(ADDR_IS_ON_EVEN_PAGE(vaddr)) {
                if(pagetable->entries[i].V0 == 0) {
                    KERNEL_PANIC("Tried to set dirty bit of an unmapped "
                                 "entry");
                } else {
                    pagetable->entries[i].D0 = dirty;
                    return;
                }
            } else {
                if(pagetable->entries[i].V1 == 0) {
                    KERNEL_PANIC("Tried to set dirty bit of an unmapped "
                                 "entry");
                } else {
                    pagetable->entries[i].D1 = dirty;
                    return;
                }
            }
        }
    }
    /* No mapping was found */

    KERNEL_PANIC("Tried to set dirty bit of an unmapped entry");
    #endif
}

/** @} */

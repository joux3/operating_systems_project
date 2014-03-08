/*
 * ELF binary format.
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
 * $Id: elf.c,v 1.3 2012/02/03 09:16:53 tlilja Exp $
 *
 */
#include "proc/elf.h"
#include "lib/libc.h"
#include "drivers/yams.h"
#ifdef CHANGED_2
    #include "lib/debug.h"
#endif

/** @name ELF loader.
 *
 * This module contains a function to parse useful information from
 * ELF headers.
 */

/**
 * Parse useful information from a given ELF file into the ELF info
 * structure.
 *
 * @param file The ELF file
 *
 * @param elf Information found in the file is returned in this
 * structure. In case of error this structure may contain arbitrary
 * values.
 *
 * @return 0 on failure, other values indicate success.
 */
int elf_parse_header(elf_info_t *elf, openfile_t file)
{
    Elf32_Ehdr elf_hdr;
    Elf32_Phdr program_hdr;

    int i;
    int current_position;
    int segs = 0;
    #if CHANGED_2
        int valid_entry_point = 0;
        char buffer[256];
        uint32_t elf_size = 0;
        int last_read;
    #endif
#define SEG_RO 1
#define SEG_RW 2

    /* Read the ELF header */
    if (vfs_read(file, &elf_hdr, sizeof(elf_hdr))
        != sizeof(elf_hdr)) {
        return 0;
    }

    /* Check that the ELF magic is correct. */
    if (elf_hdr.e_ident.i != ELF_MAGIC) {
        return 0;
    }

    /* File data is not MIPS 32 bit big-endian */
    if (elf_hdr.e_ident.c[EI_CLASS] != ELFCLASS32
        || elf_hdr.e_ident.c[EI_DATA] != ELFDATA2MSB
        || elf_hdr.e_machine != EM_MIPS) {
        return 0;
    }

    /* Invalid ELF version */
    if (elf_hdr.e_version != EV_CURRENT 
        || elf_hdr.e_ident.c[EI_VERSION] != EV_CURRENT) {
        return 0;
    }

    /* Not an executable file */
    if (elf_hdr.e_type != ET_EXEC) {
        return 0;
    }

    /* No program headers */
    if (elf_hdr.e_phnum == 0) {
        return 0;
    }

    /* Zero the return structure. Uninitialized data is bad(TM). */
    memoryset(elf, 0, sizeof(*elf));

    /* Get the entry point */
    elf->entry_point = elf_hdr.e_entry;

    /* Seek to the program header table */
    current_position = elf_hdr.e_phoff;
    vfs_seek(file, current_position);

    /* Read the program headers. */
    for (i = 0; i < elf_hdr.e_phnum; i++) {
        if (vfs_read(file, &program_hdr, sizeof(program_hdr))
            != sizeof(program_hdr)) {
            return 0;
        }

        switch (program_hdr.p_type) {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
            /* These program headers can be ignored */
            break;
        case PT_LOAD:
            /* These are the ones we are looking for */

            /* The RW segment */
            if (program_hdr.p_flags & PF_W) {
                if (segs & SEG_RW) { /* already have an RW segment*/
                    return 0;
                }
                segs |= SEG_RW;

                elf->rw_location = program_hdr.p_offset;
                elf->rw_size = program_hdr.p_filesz;
                elf->rw_vaddr = program_hdr.p_vaddr;
                #ifdef CHANGED_2
                // if we're not on page boundary, we need one extra page
                elf->rw_pages = 
                    ((program_hdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE) + 
                    (((program_hdr.p_vaddr & ~0xfff) == program_hdr.p_vaddr) ? 0 : 1);
                #else
                /* memory size rounded up to the page boundary, in pages */
                elf->rw_pages = 
                    (program_hdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
                #endif

            /* The RO segment */
            } else {
                if (segs & SEG_RO) { /* already have an RO segment*/
                    return 0;
                }
                segs |= SEG_RO; 

                elf->ro_location = program_hdr.p_offset;
                elf->ro_size = program_hdr.p_filesz;
                elf->ro_vaddr = program_hdr.p_vaddr;
                #ifdef CHANGED_2
                // if we're not on page boundary, we need one extra page
                elf->ro_pages = 
                    ((program_hdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE) + 
                    (((program_hdr.p_vaddr & ~0xfff) == program_hdr.p_vaddr) ? 0 : 1);
                #else
                /* memory size rounded up to the page boundary, in pages */
                elf->ro_pages = 
                    (program_hdr.p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
                #endif
            }

            break;
        default:
            /* Other program headers indicate an incompatible file */
            return 0;
        }

        /* In case the program header size is non-standard: */
        current_position += sizeof(program_hdr);
        vfs_seek(file, current_position);
    }

    #ifdef CHANGED_2
        /* check that the segments don't overlap in destination memory */
        if ((segs & SEG_RO) && (segs & SEG_RW)) {
            if (elf->ro_vaddr < elf->rw_vaddr) {
                if (elf->ro_vaddr + elf->ro_size >= elf->rw_vaddr)
                    return 0;
            } else {
                if (elf->rw_vaddr + elf->rw_size >= elf->ro_vaddr)
                    return 0;
            }
        }

        /* check that the entry point is inside a segment */
        if ((segs & SEG_RO) && (elf->ro_vaddr <= elf->entry_point && (elf->ro_vaddr + elf->ro_size) >= elf->entry_point)) {
            valid_entry_point = 1;
        }
        if ((segs & SEG_RW) && (elf->rw_vaddr <= elf->entry_point && (elf->rw_vaddr + elf->rw_size) >= elf->entry_point)) {
            valid_entry_point = 1;
        }
        if (!valid_entry_point)
            return 0;

        // validate ro_vaddr and rw_vaddr
        DEBUG("elfdebug", "Validating segment virtual memory areas\n");
        if ((segs & SEG_RO) && (elf->ro_vaddr < PAGE_SIZE || (elf->ro_vaddr + elf->ro_size) >= USERLAND_STACK_TOP)) {
            return 0;
        }
        if ((segs & SEG_RW) && (elf->rw_vaddr < PAGE_SIZE || (elf->rw_vaddr + elf->rw_size) >= USERLAND_STACK_TOP)) {
            return 0;
        }

        DEBUG("elfdebug", "Validating segment presence in elf file\n");
        // calculate file size
        vfs_seek(file, 0);
        while((last_read = vfs_read(file, (void*)buffer, 256)) != 0) {
            elf_size += last_read;
        }
        DEBUG("elfdebug", "Elf file size %d\n", elf_size);
        // validate that the elf segments can actually be found
        if ((segs & SEG_RO) && ((elf->ro_location + elf->ro_size) > elf_size)) {
            return 0;
        }
        if ((segs & SEG_RW) && ((elf->rw_location + elf->rw_size) > elf_size)) {
            return 0;
        }
        DEBUG("elfdebug", "Validated segment presence\n");
    #endif
    

    /* Make sure either RW or RO segment is present: */
    return (segs > 0);
}

/** @} */

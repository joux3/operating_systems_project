#ifdef CHANGED_3

#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "kernel/semaphore.h"
#include "vm/pagepool.h"
#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "fs/sfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"
#include "lib/debug.h"


/* Data structure used to represent an open file in sfs file system */

typedef struct {
    //lock guarding write
    lock_t *lock;
    //semaphore guarding concurrent readers
    semaphore_t *sem; 
    int open_count, is_deleted;
    uint32_t file_block;
} sfs_open_file_t;

/* Data structure used internally by SFS filesystem. This data structure 
   is used by sfs-functions. it is initialized during sfs_init(). Also
   memory for the buffers is reserved _dynamically_ during init.

   Buffers are used when reading/writing system or data blocks from/to 
   disk.
*/

typedef struct {
    /* Pointer to gbd device performing sfs */
    gbd_t          *disk;

    // lock for mutual exclusion of fs-operations
    lock_t    *lock;

    // block allocation block count
    uint32_t bab_count;
    uint32_t block_count;
    uint32_t data_block_count;

    // pointer to the BABs read from disk
    union {
        char *buffer;
        bitmap_t *bitmap;
    } bab_cache;
    // allocate here to save stack space
    // NOTE: do not use these when not holding sfs->lock
    union { 
        char buffer[SFS_BLOCK_SIZE];
        sfs_inode_t node;
    } inode;
    // reserve space for some indirect block pointer handling
    uint32_t indirect1[SFS_BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t indirect2[SFS_BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t indirect3[SFS_BLOCK_SIZE / sizeof(uint32_t)];
    // some raw data
    char rawbuffer[SFS_BLOCK_SIZE];
    //open files
    sfs_open_file_t open_files[SFS_MAX_OPEN_FILES];
} sfs_t;


#define SFS_BLOCKS_PER_BAB (SFS_BLOCK_SIZE * 8)

int sfs_read_block(sfs_t *sfs, uint32_t block, void *buffer) {
    gbd_request_t req;

    req.block = block;
    req.sem = NULL;
    req.buf = ADDR_KERNEL_TO_PHYS((uint32_t)buffer);
    return sfs->disk->read_block(sfs->disk, &req);
}

int sfs_write_block(sfs_t *sfs, uint32_t block, void *buffer) {
    gbd_request_t req;

    req.block = block;
    req.sem = NULL;
    req.buf = ADDR_KERNEL_TO_PHYS((uint32_t)buffer);
    return sfs->disk->write_block(sfs->disk, &req);
}

int sfs_read_bab_cache(sfs_t *sfs) {
    uint32_t i;
    char *bab_cache = sfs->bab_cache.buffer;
    DEBUG("sfsdebug", "Reading BAB cache from disk\n");
    for (i = 0; i < sfs->bab_count; i++) {
        if (sfs_read_block(sfs, 1 + i, bab_cache) == 0)
            KERNEL_PANIC("SFS: disk read failed in sfs_read_bab_cache, could lead to corruption!\n");
        bab_cache += SFS_BLOCK_SIZE;
    }
    return 1;
}

int sfs_write_bab_cache(sfs_t *sfs) {
    uint32_t i;
    char *bab_cache = sfs->bab_cache.buffer;
    DEBUG("sfsdebug", "Writing BAB cache to disk\n");
    for (i = 0; i < sfs->bab_count; i++) {
        if (sfs_write_block(sfs, 1 + i, bab_cache) == 0)
            KERNEL_PANIC("SFS: disk write failed in sfs_write_bab_cache, could have already corrupted FS!\n");
        bab_cache += SFS_BLOCK_SIZE;
    }
    return 1;
}

int sfs_is_block_free(sfs_t *sfs, uint32_t block) {
    KERNEL_ASSERT((block > sfs->bab_count) && (block + sfs->bab_count < sfs->block_count));
    return bitmap_get(sfs->bab_cache.bitmap, block - sfs->bab_count - 1) == 0;
}

void sfs_free_block(sfs_t *sfs, uint32_t block) {
    DEBUG("sfsdebug", "SFS freeing diskblock %d, data block %d\n", block, block - sfs->bab_count - 1);
    KERNEL_ASSERT((block > sfs->bab_count) && (block + sfs->bab_count < sfs->block_count));
    bitmap_set(sfs->bab_cache.bitmap, block - sfs->bab_count - 1, 0);
}

// finds a free block and marks it as used
// returns 0 if none is found
uint32_t sfs_get_free_block(sfs_t *sfs) {
    int free_block = bitmap_findnset(sfs->bab_cache.bitmap, sfs->data_block_count);
    if (free_block != -1) {
        return 1 + sfs->bab_count + (uint32_t)free_block;
    }
    return 0;
}

uint32_t sfs_root_inode(sfs_t *sfs) {
    return 1 + sfs->bab_count; 
}

/** 
 * @param Pointer to gbd-device performing sfs.
 *
 * @return Pointer to the filesystem data structure fs_t, if fails
 * return NULL. 
 */
fs_t * sfs_init(gbd_t *disk) 
{
    gbd_request_t req;
    char name[SFS_VOLUMENAME_MAX];
    fs_t *fs;
    sfs_t *sfs;
    uint32_t addr;
    uint32_t buffer[SFS_BLOCK_SIZE / sizeof(uint32_t)];
    int r;
    lock_t *lock;

    KERNEL_ASSERT(SFS_BLOCK_SIZE >= sizeof(sfs_inode_t));

    if (disk->block_size(disk) != SFS_BLOCK_SIZE)
        return NULL;

    lock = lock_create();
    if (lock == NULL) {
        kprintf("sfs_init: could not create a new lock.\n");
        return NULL;
    }

    addr = pagepool_get_phys_page();
    if (addr == 0) {
        lock_destroy(lock);
        kprintf("sfs_init: could not allocate memory.\n");
        return NULL;
    }
    addr = ADDR_PHYS_TO_KERNEL(addr);      /* transform to vm address */

    KERNEL_ASSERT(PAGE_SIZE >= sizeof(sfs_t) + sizeof(fs_t));

    // read sfs header block, check the magic
    req.block = 0;
    req.sem = NULL;
    req.buf = ADDR_KERNEL_TO_PHYS((uint32_t)&buffer);
    r = disk->read_block(disk, &req);
    if (r == 0) {
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        kprintf("sfs_init: Error during disk read. Initialization failed.\n");
        return NULL; 
    }

    if (buffer[0] != SFS_MAGIC) {
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        return NULL;
    }


    stringcopy(name, (((char*)buffer) + 4), SFS_VOLUMENAME_MAX);
    fs = (fs_t*)addr;
    sfs = (sfs_t*)(addr + sizeof(fs_t));

    int i;
    for(i = 0; i < SFS_MAX_OPEN_FILES; i++) {
        sfs->open_files[i].open_count = 0;    

    }
    sfs->disk = disk;
    sfs->lock = lock;
    sfs->block_count = disk->total_blocks(disk);
    sfs->bab_count = ((sfs->block_count - 1) + 1024)/1025;
    sfs->data_block_count = sfs->block_count - sfs->bab_count - 1;

    KERNEL_ASSERT(PAGE_SIZE >= sizeof(sfs_t) + sizeof(fs_t) + sfs->bab_count * SFS_BLOCK_SIZE);

    sfs->bab_cache.buffer = (char*)(addr + sizeof(fs_t) + sizeof(sfs_t));
    if (sfs_read_bab_cache(sfs) == 0) {
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        kprintf("sfs_init: Failed to read Block Allocation Blocks.\n");
        return NULL;
    }

    DEBUG("sfsdebug", "SFS: Found %d BABs, %d total blocks\n", sfs->bab_count, sfs->block_count);

    // do some quick sanity checks:
    // - check that the first data block is always marked as used (as it's reserved for root inode)
    if (sfs_is_block_free(sfs, sfs_root_inode(sfs)) != 0) {
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        kprintf("sfs_init: Sanity check: root dir node marked free!? Initialization failed.\n");
        return NULL;
    }
    // - check that the root inode is a directory
    r = sfs_read_block(sfs, sfs_root_inode(sfs), &buffer);
    if (r == 0 || ((sfs_inode_t*)buffer)->inode_type != SFS_DIR_INODE) {
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        kprintf("sfs_init: Sanity check: root dir inode not marked as dir! Initialization failed.\n");
        return NULL;
    }

    fs->internal = (void*)sfs; 
    stringcopy(fs->volume_name, name, VFS_NAME_LENGTH);

    fs->unmount = sfs_unmount;
    fs->open    = sfs_open;
    fs->close   = sfs_close;
    fs->create  = sfs_create;
    fs->remove  = sfs_remove;
    fs->read    = sfs_read;
    fs->write   = sfs_write;
    fs->getfree = sfs_getfree;

    return fs;
}


/**
 * Unmounts sfs filesystem from gbd device. After this SFS-driver and
 * gbd-device are no longer linked together. Implements
 * fs.unmount(). Waits for the current operation(s) to finish, frees
 * reserved memory and returns OK.
 *
 * @param fs Pointer to fs data structure of the device.
 *
 * @return VFS_OK
 */
int sfs_unmount(fs_t *fs) 
{
    int i;
    sfs_t *sfs;
    sfs = (sfs_t*)fs->internal;

    // close all open files
    for(i = 0; i < SFS_MAX_OPEN_FILES; i++) {
        if(sfs->open_files[i].open_count > 0) {
            sfs_close(fs, i);
        }
    }
    lock_acquire(sfs->lock); 
    lock_destroy(sfs->lock); 

    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS((uint32_t)fs));
    return VFS_OK;
}

// returns:
// - the file inode block number if found
// - 0 if not found
uint32_t sfs_find_file_and_dir(sfs_t *sfs, char *filename, uint32_t *dir_block) 
{
    int r;
    uint32_t cur_dir_block, i;
    sfs_inode_dir_t *dir_inode;
    cur_dir_block = sfs_root_inode(sfs); 
    while (1) {
        r = sfs_read_block(sfs, cur_dir_block, &(sfs->inode.buffer));
        if (r == 0) {
            return 0;
        }
        if (sfs->inode.node.inode_type != SFS_DIR_INODE) {
            kprintf("SFS: inode should be dir but isn't! inode %d\n", cur_dir_block);
            KERNEL_PANIC("SFS: corrupted filesystem!\n");
        }
        dir_inode = &(sfs->inode.node.dir);
        for (i = 0; i < SFS_ENTRIES_PER_DIR; i++) {
            if (dir_inode->entries[i].inode > 0 && stringcmp(dir_inode->entries[i].name, filename) == 0) {
                if (dir_block != NULL)
                    *dir_block = cur_dir_block;
                // TODO: check that the inode is actually a file inode, with dir support it might be a dir
                return dir_inode->entries[i].inode;
            }
        } 
        if (dir_inode->next_dir_inode == 0) {
            return 0;
        } else {
            cur_dir_block = dir_inode->next_dir_inode;
        }
    }
}

uint32_t sfs_find_file(sfs_t *sfs, char *filename) {
    return sfs_find_file_and_dir(sfs, filename, NULL);
}

//return incex to table or -1 if not found
int sfs_find_open_file(sfs_t *sfs, uint32_t header_block) {
    int i = 0;
    for(i = 0; i < SFS_MAX_OPEN_FILES; i++) {
        if(sfs->open_files[i].open_count > 0 && sfs->open_files[i].file_block == header_block)
            return i;
    }
    return -1;
}

/**
 * Opens file. Implements fs.open(). Reads directory block of sfs
 * device and finds given file. Returns file's inode block number or
 * VFS_NOT_FOUND, if file not found.
 * 
 * @param fs Pointer to fs data structure of the device.
 * @param filename Name of the file to be opened.
 *
 * @return If file found, return inode block number as fileid, otherwise
 * return VFS_NOT_FOUND.
 */
int sfs_open(fs_t *fs, char *filename) {
    sfs_t *sfs = fs->internal;
    int i, index;
    lock_acquire(sfs->lock);
    uint32_t file_inode = sfs_find_file(sfs, filename);
    DEBUG("sfsdebug", "SFS_open: file block %d, name %s\n", file_inode, filename);
    if (file_inode == 0)
        goto error; 

    index = -1;
    //find if file is already open
    index = sfs_find_open_file(sfs, file_inode);
    DEBUG("sfsdebug", "SFS_open: file %s open in index %d\n", filename, index);
    //file not open so initialize new open file
    if(index < 0) {
        for(i = 0; i < SFS_MAX_OPEN_FILES; i++) {
            if(sfs->open_files[i].open_count == 0) {
                index = i;
                break;
            }
        }
	DEBUG("sfsdebug", "SFS_open: file %s not yet opening at index %d\n", filename, index);
        //no open spots found
        if(index < 0) 
            goto error;

        sfs_open_file_t f;
        f.lock = lock_create();
        if(f.lock == NULL)
            goto error;
        f.sem = semaphore_create(SFS_MAX_READERS);
        if(f.sem == NULL) {
            lock_destroy(f.lock);
            goto error;
        }
        f.is_deleted = 0;
        f.file_block = file_inode;
        f.open_count = 0;
        sfs->open_files[index] = f;
    }
    sfs->open_files[index].open_count++;
    lock_release(sfs->lock);
    DEBUG("sfsdebug", "SFS_open: file %s opened successfully\n", filename);
    return index;

    error:
    lock_release(sfs->lock);
    DEBUG("sfsdebug", "SFS_open: file %s open failed\n", filename);
    return VFS_ERROR;
}




// reserves direct blocks to pointers
// returns:
// - the size_left after reserving
// - -1 if a block reservation fails
int sfs_reserve_direct_blocks(sfs_t *sfs, int size_left, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    memoryset(&(sfs->rawbuffer), 0, SFS_BLOCK_SIZE);
    for (i = 0; i < max_blocks && size_left > 0; i++) {
        uint32_t direct_block = sfs_get_free_block(sfs);
        DEBUG("sfsdebug", "reserving block %d for direct file data\n", direct_block);
        if (direct_block == 0)
            return -1;
        // empty out the new file data block
        KERNEL_ASSERT(sfs_write_block(sfs, direct_block, &(sfs->rawbuffer)) != 0);
        size_left -= MIN(size_left, SFS_BLOCK_SIZE);
        pointers[i] = direct_block;
    }
    return size_left;
}

int sfs_reserve_indirect1_blocks(sfs_t *sfs, int size_left, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    memoryset(&(sfs->indirect1), 0, SFS_BLOCK_SIZE);
    for (i = 0; i < max_blocks && size_left > 0; i++) {
        uint32_t indirect1_block = sfs_get_free_block(sfs);
        DEBUG("sfsdebug", "reserving block %d for indirect1 pointer data\n", indirect1_block);
        if (indirect1_block == 0)
            return -1;
        size_left = sfs_reserve_direct_blocks(sfs, size_left, (uint32_t*)&(sfs->indirect1), SFS_INDIRECT_POINTERS); 
        KERNEL_ASSERT(sfs_write_block(sfs, indirect1_block, &(sfs->indirect1)) != 0);
        pointers[i] = indirect1_block;
        
    }
    return size_left;
}

int sfs_reserve_indirect2_blocks(sfs_t *sfs, int size_left, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    memoryset(&(sfs->indirect2), 0, SFS_BLOCK_SIZE);
    for (i = 0; i < max_blocks && size_left > 0; i++) {
        uint32_t indirect2_block = sfs_get_free_block(sfs);
        DEBUG("sfsdebug", "reserving block %d for indirect2 pointer data\n", indirect2_block);
        if (indirect2_block == 0)
            return -1;
        size_left = sfs_reserve_indirect1_blocks(sfs, size_left, (uint32_t*)&(sfs->indirect2), SFS_INDIRECT_POINTERS); 
        KERNEL_ASSERT(sfs_write_block(sfs, indirect2_block, &(sfs->indirect2)) != 0);
        pointers[i] = indirect2_block;
    }
    return size_left;
}

int sfs_reserve_indirect3_blocks(sfs_t *sfs, int size_left, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    memoryset(&(sfs->indirect3), 0, SFS_BLOCK_SIZE);
    for (i = 0; i < max_blocks && size_left > 0; i++) {
        uint32_t indirect3_block = sfs_get_free_block(sfs);
        DEBUG("sfsdebug", "reserving block %d for indirect3 pointer data\n", indirect3_block);
        if (indirect3_block == 0)
            return -1;
        size_left = sfs_reserve_indirect2_blocks(sfs, size_left, (uint32_t*)&(sfs->indirect3), SFS_INDIRECT_POINTERS); 
        KERNEL_ASSERT(sfs_write_block(sfs, indirect3_block, &(sfs->indirect3)) != 0);
        pointers[i] = indirect3_block;
    }
    return size_left;
}

/**
 * Creates file of given size. Implements fs.create(). Checks that
 * file name doesn't allready exist in directory block.Allocates
 * enough blocks from the allocation block for the file (1 for inode
 * and then enough for the file of given size). Reserved blocks are zeroed.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param filename File name of the file to be created
 * @param size Size of the file to be created
 *
 * @return If file allready exists or not enough space return VFS_ERROR,
 * otherwise return VFS_OK.
 */
int sfs_create(fs_t *fs, char *filename, int size) 
{
    sfs_t *sfs = (sfs_t*)fs->internal;
    int r, i;
    uint32_t cur_dir_block, dir_inode_with_free_entry;
    sfs_inode_dir_t *dir_inode;
    dir_inode_with_free_entry = 0;
     
    if ((uint32_t)size > SFS_MAX_FILESIZE || size < 0) {
        return VFS_ERROR;
    }

    lock_acquire(sfs->lock);

    // check if the file exists or not
    cur_dir_block = sfs_root_inode(sfs); 
    while (1) {
        r = sfs_read_block(sfs, cur_dir_block, &(sfs->inode.buffer));
        if (r == 0) {
            lock_release(sfs->lock);
            return VFS_ERROR;
        }
        if (sfs->inode.node.inode_type != SFS_DIR_INODE) {
            kprintf("SFS: inode should be dir but isn't! inode %d\n", cur_dir_block);
            KERNEL_PANIC("SFS: corrupted filesystem!\n");
        }
        dir_inode = &(sfs->inode.node.dir);
        for (i = 0; i < (int)SFS_ENTRIES_PER_DIR; i++) {
            if (dir_inode->entries[i].inode == 0 && dir_inode_with_free_entry == 0) {
                dir_inode_with_free_entry = cur_dir_block;
            }
            if (dir_inode->entries[i].inode > 0 && stringcmp(dir_inode->entries[i].name, filename) == 0) {
                DEBUG("sfsdebug", "SFS sfs_create: File %s already exists!\n", filename);
                lock_release(sfs->lock);
                return VFS_ERROR;
            }
        } 
        if (dir_inode->next_dir_inode == 0) {
            break;
        } else {
            cur_dir_block = dir_inode->next_dir_inode;
        }
    }

    // we haven't encountered a free dir entry yet, first check the whole chain
    DEBUG("sfsdebug", "dir block with free entry %d\n", dir_inode_with_free_entry);
    while (dir_inode_with_free_entry == 0) {
        dir_inode = &(sfs->inode.node.dir);
        for (i = 0; i < (int)SFS_ENTRIES_PER_DIR; i++) {
            if (dir_inode->entries[i].inode == 0) {
                dir_inode_with_free_entry = cur_dir_block;
                break;
            }
        } 
        if (dir_inode_with_free_entry != 0) { // already found
            break;
        }
        // follow the chained directories
        if (dir_inode->next_dir_inode == 0) {
            DEBUG("sfsdebug", "SFS sfs_create: creating new dir entry since others are full\n");
            // end of chain, need to reserve a new dir block
            dir_inode_with_free_entry = sfs_get_free_block(sfs);
            if (dir_inode_with_free_entry == 0) {
                DEBUG("sfsdebug", "SFS sfs_create: failed, disk full\n");
                lock_release(sfs->lock);
                return VFS_ERROR;
            }
            dir_inode->next_dir_inode = dir_inode_with_free_entry;
            DEBUG("sfsdebug", "  -> new dir at block %d, updating block %d\n", dir_inode_with_free_entry, cur_dir_block);
            sfs_write_block(sfs, cur_dir_block, &(sfs->inode.buffer));
            memoryset(&(sfs->inode.buffer), 0, SFS_BLOCK_SIZE);
            sfs->inode.node.inode_type = SFS_DIR_INODE;
            sfs_write_block(sfs, dir_inode_with_free_entry, &(sfs->inode.buffer));
            break;
        } else {
            // go to the next dir inode
            cur_dir_block = dir_inode->next_dir_inode;

            r = sfs_read_block(sfs, cur_dir_block, &(sfs->inode.buffer));
            if (r == 0) {
                lock_release(sfs->lock);
                return VFS_ERROR;
            }
            if (sfs->inode.node.inode_type != SFS_DIR_INODE) {
                kprintf("SFS: inode should be dir but isn't! inode %d\n", cur_dir_block);
                KERNEL_PANIC("SFS: corrupted filesystem!\n");
            }
        }
    }

    DEBUG("sfsdebug", "SFS sfs_create: ok, creating file %s, dir %d\n", filename, dir_inode_with_free_entry);

    // reserve & populate blocks for the file
    // if any of the sfs_get_free_blocks fails, free all the already reserved blocks
    // by calling sfs_read_bab_cache

    uint32_t file_block = sfs_get_free_block(sfs);
    if (file_block == 0) {
        DEBUG("sfsdebug", "SFS sfs_create: failed, disk full\n");
        sfs_read_bab_cache(sfs);
        lock_release(sfs->lock);
        return VFS_ERROR;
    }
    memoryset(&(sfs->inode.buffer), 0, SFS_BLOCK_SIZE);
    sfs->inode.node.inode_type = SFS_FILE_INODE; 
    sfs->inode.node.file.filesize = size;

    // reserve enough space for the file:

    int size_left = size;

    // - direct blocks
    size_left = sfs_reserve_direct_blocks(sfs, size_left, (uint32_t*)&(sfs->inode.node.file.direct_blocks), SFS_DIRECT_DATA_BLOCKS);

    // - first indirect blocks
    if (size_left > 0)
        size_left = sfs_reserve_indirect1_blocks(sfs, size_left, &(sfs->inode.node.file.first_indirect), 1);
    // - second indirect blocks
    if (size_left > 0)
        size_left = sfs_reserve_indirect2_blocks(sfs, size_left, &(sfs->inode.node.file.second_indirect), 1);
    if (size_left > 0) 
        size_left = sfs_reserve_indirect3_blocks(sfs, size_left, &(sfs->inode.node.file.third_indirect), 1);
    DEBUG("sfsdebug", "SFS_create: block reservation done with size_left %d\n", size_left);

    // if any of the block reservations failed (i.e. disk got full), rollback the block reservations
    // by reading BABs from the disk again
    if (size_left == -1) {  
        DEBUG("sfsdebug", "SFS sfs_create: failed, disk full\n");
        sfs_read_bab_cache(sfs);
        lock_release(sfs->lock);
        return VFS_ERROR;
    }

    KERNEL_ASSERT(size_left == 0);

    // write the file block
    if (sfs_write_block(sfs, file_block, &(sfs->inode.buffer)) == 0) {
        sfs_read_bab_cache(sfs);
        lock_release(sfs->lock);
        return VFS_ERROR;
    }

    // required blocks for the file have been reserved, now add it to the directory inode
     
    r = sfs_read_block(sfs, dir_inode_with_free_entry, &(sfs->inode.buffer));
    if (r == 0) {
        sfs_read_bab_cache(sfs);
        lock_release(sfs->lock);
        return VFS_ERROR;
    }
    if (sfs->inode.node.inode_type != SFS_DIR_INODE) {
        kprintf("SFS: inode should be dir but isn't! inode %d\n", dir_inode_with_free_entry);
        KERNEL_PANIC("SFS: corrupted filesystem!\n");
    }
    dir_inode = &(sfs->inode.node.dir);
    for (i = 0; i < (int)SFS_ENTRIES_PER_DIR; i++) {
        if (dir_inode->entries[i].inode == 0) {
            dir_inode->entries[i].inode = file_block; 
            stringcopy(dir_inode->entries[i].name, filename, SFS_FILENAME_MAX);
            r = sfs_write_block(sfs, dir_inode_with_free_entry, &(sfs->inode.buffer));
            if (r == 0) {
                sfs_read_bab_cache(sfs);
                lock_release(sfs->lock);
                return VFS_ERROR;
            } else {
                // persist the allocated blocks
                sfs_write_bab_cache(sfs);
                lock_release(sfs->lock);
                return VFS_OK;
            }
        }
    }
    KERNEL_PANIC("SFS: could not find empty directory entry even though that should be guaranteed!\n");
    return VFS_ERROR;
}

// frees direct data blocks
void sfs_free_direct_blocks(sfs_t *sfs, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    for (i = 0; i < max_blocks; i++)
        if (pointers[i] != 0)
            sfs_free_block(sfs, pointers[i]);
}

int sfs_free_indirect1_blocks(sfs_t *sfs, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    for (i = 0; i < max_blocks; i++) {
        if (pointers[i] != 0) {
            if (sfs_read_block(sfs, pointers[i], &(sfs->indirect1)) == 0) 
                return 0;
            sfs_free_direct_blocks(sfs, (uint32_t*)&(sfs->indirect1), SFS_INDIRECT_POINTERS);
            sfs_free_block(sfs, pointers[i]);
        }
    }
    return 1;
}

int sfs_free_indirect2_blocks(sfs_t *sfs, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    for (i = 0; i < max_blocks; i++) {
        if (pointers[i] != 0) {
            if (sfs_read_block(sfs, pointers[i], &(sfs->indirect2)) == 0) 
                return 0;
            if (sfs_free_indirect1_blocks(sfs, (uint32_t*)&(sfs->indirect2), SFS_INDIRECT_POINTERS) == 0)
                return 0;
            sfs_free_block(sfs, pointers[i]);
        }
    }
    return 1;
}

int sfs_free_indirect3_blocks(sfs_t *sfs, uint32_t *pointers, uint32_t max_blocks) {
    uint32_t i;
    for (i = 0; i < max_blocks; i++) {
        if (pointers[i] != 0) {
            if (sfs_read_block(sfs, pointers[i], &(sfs->indirect3)) == 0) 
                return 0;
            if (sfs_free_indirect2_blocks(sfs, (uint32_t*)&(sfs->indirect3), SFS_INDIRECT_POINTERS) == 0)
                return 0;
            sfs_free_block(sfs, pointers[i]);
        }
    }
    return 1;
}

// frees all the data blocks & the file block itself
// returns:
// - 1 if no error
// - 0 if error occured reading any of the file blocks
int sfs_free_file_blocks(sfs_t *sfs, uint32_t file_block) 
{
    if (sfs_read_block(sfs, file_block, &(sfs->inode.buffer)) == 0) 
        return 0;
    sfs_free_block(sfs, file_block);
    sfs_free_direct_blocks(sfs, (uint32_t*)&(sfs->inode.node.file.direct_blocks), SFS_DIRECT_DATA_BLOCKS);
    if (sfs_free_indirect1_blocks(sfs, &(sfs->inode.node.file.first_indirect), 1) == 0)
        return 0;
    if (sfs_free_indirect2_blocks(sfs, &(sfs->inode.node.file.second_indirect), 1) == 0)
        return 0;
    if (sfs_free_indirect3_blocks(sfs, &(sfs->inode.node.file.third_indirect), 1) == 0)
        return 0;

    return 1;
}

/**
 * Closes file. Implements fs.close()
 *
 * @param fs Pointer to fs data structure of the device.
 * @param fileid File id (inode block number) of the file.
 *
 * @return VFS_OK
 */
int sfs_close(fs_t *fs, int fileid)
{
    sfs_open_file_t *f;
    sfs_t *sfs = (sfs_t*)fs->internal;
    lock_acquire(sfs->lock);

    f = &(sfs->open_files[fileid]);
    f->open_count--;
    //if file is deleted and this is the last process to close it delete file
    if(f->is_deleted && f->open_count == 0) {
        if(sfs_free_file_blocks(sfs, f->file_block) == 0) {
            lock_release(sfs->lock);
            return VFS_ERROR;	
        }
        sfs_write_bab_cache(sfs);

        lock_acquire(f->lock);
        int i;
        for(i = 0; i < SFS_MAX_READERS; i++) {
            semaphore_P(f->sem);
        }
        semaphore_destroy(f->sem);
        lock_destroy(f->lock);
    }
    lock_release(sfs->lock);
    return VFS_OK;
}

/**
 * Removes given file. Implements fs.remove(). Frees blocks allocated
 * for the file and directory entry.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param filename file to be removed.
 *
 * @return VFS_OK if file succesfully removed. If file not found
 * VFS_NOT_FOUND.
 */

int sfs_remove(fs_t *fs, char *filename) 
{
    int i;

    sfs_t *sfs = fs->internal;
    lock_acquire(sfs->lock);

    uint32_t dir_block;
    uint32_t file_block = sfs_find_file_and_dir(sfs, filename, &dir_block);
    if (file_block == 0)
        goto error;
    
    // read the directory block free the entry with the given filename
    if (sfs_read_block(sfs, dir_block, &(sfs->inode.buffer)) == 0)
        goto error;
    for (i = 0; i < (int)SFS_ENTRIES_PER_DIR; i++) {
        if (stringcmp(sfs->inode.node.dir.entries[i].name, filename) == 0) {
            sfs->inode.node.dir.entries[i].inode = 0; 
            break;
        } else if (i == SFS_ENTRIES_PER_DIR - 1) {
            KERNEL_PANIC("SFS: sfs_find_file_and_dir inconsistency!\n");
        }
    }
    if (sfs_write_block(sfs, dir_block, &(sfs->inode.buffer)) == 0) 
        goto error;
    
    int open_index = sfs_find_open_file(sfs, file_block);
    //if file is not open by anyone free all blocks
    if(open_index < 0) {
        if (sfs_free_file_blocks(sfs, file_block) == 0) 
            goto error;
    }
    //else mark it as deleted
    else {
        sfs->open_files[open_index].is_deleted = 1;
    }

    sfs_write_bab_cache(sfs);
    lock_release(sfs->lock);
    return VFS_OK;
error:
    lock_release(sfs->lock);
    return VFS_ERROR;
}

int sfs_read_direct_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *bufsize, int *offset, int base_offset) {
    int block;
    int read = 0;
    for (block = 0; block < pointer_count && *bufsize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_BLOCK_SIZE) {
            // read touches this block
            DEBUG("sfsdebug", "Reading block %d\n", pointers[block]);
            if (sfs_read_block(sfs, pointers[block], raw_buffer) == 0) 
                return -1;
            // copy to the buffer
            int in_this_block = MIN((int)SFS_BLOCK_SIZE - ((*offset - base_offset) % (int)SFS_BLOCK_SIZE), *bufsize);
            memcopy(in_this_block, *buffer, raw_buffer + ((*offset - base_offset) % (int)SFS_BLOCK_SIZE)); 
            read += in_this_block;
            *bufsize -= in_this_block;
            *offset += in_this_block;
            *buffer = (void*)((uint32_t)*buffer + in_this_block);
        }
    }
    return read;
}

int sfs_read_indirect1_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *bufsize, int *offset, int base_offset, uint32_t *indirect1) {
    int block, read = 0;
    for (block = 0; block < pointer_count && *bufsize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE) {
            if (sfs_read_block(sfs, pointers[block], indirect1) == 0)
                return -1;
    
            int res = sfs_read_direct_blocks(sfs, buffer, raw_buffer, indirect1, SFS_INDIRECT_POINTERS, bufsize, offset, base_offset + block * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE);
            if (res == -1)
                return -1;
            read += res;
        }
    }
    return read;
}

int sfs_read_indirect2_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *bufsize, int *offset, int base_offset, uint32_t *indirect1, uint32_t *indirect2) {
    int block, read = 0;
    for (block = 0; block < pointer_count && *bufsize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE) {
            if (sfs_read_block(sfs, pointers[block], indirect2) == 0)
                return -1;
    
            int res = sfs_read_indirect1_blocks(sfs, buffer, raw_buffer, indirect2, SFS_INDIRECT_POINTERS, bufsize, offset, base_offset + block * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE, indirect1);
            if (res == -1)
                return -1;
            read += res;
        }
    }
    return read;
}

int sfs_read_indirect3_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *bufsize, int *offset, int base_offset, uint32_t *indirect1, uint32_t *indirect2, uint32_t *indirect3) {
    int block, read = 0;
    for (block = 0; block < pointer_count && *bufsize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE) {
            if (sfs_read_block(sfs, pointers[block], indirect3) == 0)
                return -1;
    
            int res = sfs_read_indirect2_blocks(sfs, buffer, raw_buffer, indirect3, SFS_INDIRECT_POINTERS, bufsize, offset, base_offset + block * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE, indirect1, indirect2);
            if (res == -1)
                return -1;
            read += res;
        }
    }
    return read;
}

/**
 * Reads at most bufsize bytes from file to the buffer starting from
 * the offset. bufsize bytes is always read if possible. Returns
 * number of bytes read. Buffer size must be atleast bufsize.
 * Implements fs.read().
 * 
 * @param fs  Pointer to fs data structure of the device.
 * @param fileid Fileid of the file. 
 * @param buffer Pointer to the buffer the data is read into.
 * @param bufsize Maximum number of bytes to be read.
 * @param offset Start position of reading.
 *
 * @return Number of bytes read into buffer, or VFS_ERROR if error 
 * occured.
 */ 
int sfs_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset)
{
    sfs_t *sfs = fs->internal;
    sfs_open_file_t* f = &(sfs->open_files[fileid]);
    semaphore_P(f->sem);

    DEBUG("sfsdebug", "SFS_read: offset %d, size %d\n", offset, bufsize);

    KERNEL_ASSERT(!(f->file_block <= sfs->bab_count || f->file_block >= sfs->block_count));

    uint32_t addr = pagepool_get_phys_page();
    if (addr == 0) {
        semaphore_V(f->sem);
        return VFS_ERROR;
    }
    addr = ADDR_PHYS_TO_KERNEL(addr);
    sfs_inode_t *inode = (sfs_inode_t*)addr;
    uint32_t *indirect1 = (uint32_t*)(addr + sizeof(sfs_inode_t));
    uint32_t *indirect2 = (uint32_t*)(addr + sizeof(sfs_inode_t) + 1 * SFS_BLOCK_SIZE);
    uint32_t *indirect3 = (uint32_t*)(addr + sizeof(sfs_inode_t) + 2 * SFS_BLOCK_SIZE);
    char *raw_buffer    = (char*)(addr + sizeof(sfs_inode_t) + 3 * SFS_BLOCK_SIZE);
    int read = 0;

    if (sfs_read_block(sfs, f->file_block, inode) == 0)
        goto error; 
    if (offset < 0 || offset > (int)inode->file.filesize) 
        goto error;

    bufsize = MIN(bufsize, (int)inode->file.filesize - offset);
    if (bufsize == 0) {
        goto success;
    }

    KERNEL_ASSERT(offset + bufsize <= (int)SFS_MAX_FILESIZE); 
    int tmp;
    if (offset <= (int)SFS_DIRECT_SIZE) {
        tmp = sfs_read_direct_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.direct_blocks), SFS_DIRECT_DATA_BLOCKS, &bufsize, &offset, 0);
        if (tmp == -1)
            goto error;
        read += tmp;
    }
    if (offset <= (int)SFS_FIRST_INDIRECT_SIZE && bufsize > 0) {
        tmp = sfs_read_indirect1_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.first_indirect), 1, &bufsize, &offset, (int)SFS_DIRECT_SIZE, indirect1);
        if (tmp == -1)
            goto error;
        read += tmp;
    }
    if (offset <= (int)SFS_SECOND_INDIRECT_SIZE && bufsize > 0) {
        tmp = sfs_read_indirect2_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.second_indirect), 1, &bufsize, &offset, (int)SFS_FIRST_INDIRECT_SIZE, indirect1, indirect2);
        if (tmp == -1)
            goto error;
        read += tmp;
    }
    if (bufsize > 0) {
        tmp = sfs_read_indirect3_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.third_indirect), 1, &bufsize, &offset, (int)SFS_SECOND_INDIRECT_SIZE, indirect1, indirect2, indirect3);
        if (tmp == -1)
            goto error;
        read += tmp;
    }
    KERNEL_ASSERT(bufsize == 0);

success:
    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
    semaphore_V(f->sem);
    return read;
error:
    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
    semaphore_V(f->sem);
    return VFS_ERROR;
}


// writes direct block
int sfs_write_direct_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *datasize, int *offset, int base_offset) {
    int block, written = 0;
    for (block = 0; block < pointer_count && *datasize > 0; block++) {
        if (*offset - base_offset >= block * SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * SFS_BLOCK_SIZE) {
            // write touches this block
            if (!(*offset - base_offset == block * (int)SFS_BLOCK_SIZE && *datasize >= SFS_BLOCK_SIZE)) {
                // we're not fully overwriting the block; read it first
                DEBUG("sfsdebug", "Reading block %d before overwriting parts\n", pointers[block]);
                if (sfs_read_block(sfs, pointers[block], raw_buffer) == 0) 
                    return -1;
            }
            // copy to the buffer
            int in_this_block = MIN((int)SFS_BLOCK_SIZE - ((*offset - base_offset) % (int)SFS_BLOCK_SIZE), *datasize);
            memcopy(in_this_block, raw_buffer + ((*offset - base_offset) % (int)SFS_BLOCK_SIZE), *buffer); 
            DEBUG("sfsdebug", "Writing block %d, %d new bytes\n", pointers[block], in_this_block);
            if (sfs_write_block(sfs, pointers[block], raw_buffer) == 0) 
                return -1;
            written += in_this_block;
            *datasize -= in_this_block;
            *offset += in_this_block;
            *buffer = (void*)((uint32_t)*buffer + in_this_block);
        }
    }
    return written;
}

int sfs_write_indirect1_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *datasize, int *offset, int base_offset, uint32_t *indirect1) {
    int block, written = 0;
    for (block = 0; block < pointer_count && *datasize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE) {
            if (sfs_read_block(sfs, pointers[block], indirect1) == 0)
                return -1;
    
            int res = sfs_write_direct_blocks(sfs, buffer, raw_buffer, indirect1, SFS_INDIRECT_POINTERS, datasize, offset, base_offset + block * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE);
            if (res == -1)
                return -1;
            written += res;
        }
    }
    return written;
}

int sfs_write_indirect2_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *datasize, int *offset, int base_offset, uint32_t *indirect1, uint32_t *indirect2) {
    int block, written = 0;
    for (block = 0; block < pointer_count && *datasize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE) {
            if (sfs_read_block(sfs, pointers[block], indirect2) == 0)
                return -1;
    
            int res = sfs_write_indirect1_blocks(sfs, buffer, raw_buffer, indirect2, SFS_INDIRECT_POINTERS, datasize, offset, base_offset + block * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE, indirect1);
            if (res == -1)
                return -1;
            written += res;
        }
    }
    return written;
}

int sfs_write_indirect3_blocks(sfs_t *sfs, void **buffer, char *raw_buffer, uint32_t *pointers, int pointer_count, int *datasize, int *offset, int base_offset, uint32_t *indirect1, uint32_t *indirect2, uint32_t *indirect3) {
    int block, written = 0;
    for (block = 0; block < pointer_count && *datasize > 0; block++) {
        if (*offset - base_offset >= block * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE && *offset - base_offset < (block + 1) * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_INDIRECT_POINTERS * (int)SFS_BLOCK_SIZE) {
            if (sfs_read_block(sfs, pointers[block], indirect3) == 0)
                return -1;
    
            int res = sfs_write_indirect2_blocks(sfs, buffer, raw_buffer, indirect3, SFS_INDIRECT_POINTERS, datasize, offset, base_offset + block * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE, indirect1, indirect2);
            if (res == -1)
                return -1;
            written += res;
        }
    }
    return written;
}


/**
 * Write at most datasize bytes from buffer to the file starting from
 * the offset. datasize bytes is always written if possible. Returns
 * number of bytes written. Buffer size must be atleast datasize.
 * Implements fs.read().
 * 
 * @param fs  Pointer to fs data structure of the device.
 * @param fileid Fileid of the file. 
 * @param buffer Pointer to the buffer the data is written from.
 * @param datasize Maximum number of bytes to be written.
 * @param offset Start position of writing.
 *
 * @return Number of bytes written into buffer, or VFS_ERROR if error 
 * occured.
 */ 
int sfs_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset)
{
    int i;
    int retval = 0;
    sfs_t *sfs = fs->internal;
    
    DEBUG("sfsdebug", "SFS_write: file with handle %d. offset %d, len %d datasize\n", fileid, offset, datasize);
    sfs_open_file_t* f = &(sfs->open_files[fileid]);
    DEBUG("sfsdebug", "SFS_write: file with handle %d open_count %d\n", fileid, f->open_count);
    //lock this up so that only one thread can access the semaphore
    lock_acquire(f->lock);
    //starve all the readers out
    for(i = 0; i < SFS_MAX_READERS; i++) {
        semaphore_P(f->sem);
    }
    KERNEL_ASSERT(!(f->file_block <= sfs->bab_count || f->file_block >= sfs->block_count));

    uint32_t addr = pagepool_get_phys_page();
    if (addr == 0) {
        retval = VFS_ERROR;
        goto exit2;
    }
    addr = ADDR_PHYS_TO_KERNEL(addr);
    sfs_inode_t *inode = (sfs_inode_t*)addr;
    uint32_t *indirect1 = (uint32_t*)(addr + sizeof(sfs_inode_t));
    uint32_t *indirect2 = (uint32_t*)(addr + sizeof(sfs_inode_t) + 1 * SFS_BLOCK_SIZE);
    uint32_t *indirect3 = (uint32_t*)(addr + sizeof(sfs_inode_t) + 2 * SFS_BLOCK_SIZE);
    char *raw_buffer    = (char*)(addr + sizeof(sfs_inode_t) + 3 * SFS_BLOCK_SIZE);

    if (sfs_read_block(sfs, f->file_block, inode) == 0) {
        retval = VFS_ERROR;
        goto exit1;
    }
    if (offset < 0 || offset > (int)inode->file.filesize) {
        retval = VFS_ERROR;
        goto exit1;
    }
    datasize = MIN(datasize, (int)inode->file.filesize - offset);
    if (datasize == 0) {
        goto exit1;
    }

    KERNEL_ASSERT(offset + datasize <= (int)SFS_MAX_FILESIZE);
    int tmp;
    if (offset <= (int)SFS_DIRECT_SIZE) {
        tmp = sfs_write_direct_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.direct_blocks), SFS_DIRECT_DATA_BLOCKS, &datasize, &offset, 0);
        if (tmp == -1) {
            retval = VFS_ERROR;
            goto exit1;
        }
        retval += tmp;
    }
    if (offset <= (int)SFS_FIRST_INDIRECT_SIZE && datasize > 0) {
        tmp = sfs_write_indirect1_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.first_indirect), 1, &datasize, &offset, (int)SFS_DIRECT_SIZE, indirect1);
        if (tmp == -1) {
            retval = VFS_ERROR;
            goto exit1;
        }
        retval += tmp;
    }
    if (offset <= (int)SFS_SECOND_INDIRECT_SIZE && datasize > 0) {
        tmp = sfs_write_indirect2_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.second_indirect), 1, &datasize, &offset, (int)SFS_FIRST_INDIRECT_SIZE, indirect1, indirect2);
        if (tmp == -1) {
            retval = VFS_ERROR;
            goto exit1;
        }
        retval += tmp;
    }
    if (datasize > 0) {
        tmp = sfs_write_indirect3_blocks(sfs, &buffer, raw_buffer, (uint32_t*)&(inode->file.third_indirect), 1, &datasize, &offset, (int)SFS_SECOND_INDIRECT_SIZE, indirect1, indirect2, indirect3);
        if (tmp == -1) {
            retval = VFS_ERROR;
            goto exit1;
        }
        retval += tmp;
    }
    KERNEL_ASSERT(datasize == 0);
    
exit1:
    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
exit2:
    //free all readers 
    for(i = 0; i < SFS_MAX_READERS; i++) {
        semaphore_V(f->sem);
    }
    lock_release(f->lock);
    return retval;
}

/**
 * Get number of free bytes on the disk. Implements fs.getfree().
 * Reads allocation blocks and counts number of zeros in the bitmap.
 * Result is multiplied by the block size and returned.
 *
 * @param fs Pointer to the fs data structure of the device.
 *
 * @return Number of free bytes.
 */
int sfs_getfree(fs_t *fs)
{
    uint32_t i;
    uint32_t free_blocks = 0;
    sfs_t *sfs = fs->internal;
    lock_acquire(sfs->lock);
    
    for (i = 0; i < sfs->data_block_count; i++) {
        free_blocks += bitmap_get(sfs->bab_cache.bitmap, i);
    }

    lock_release(sfs->lock);
    return free_blocks * SFS_BLOCK_SIZE;
}

#endif

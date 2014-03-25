#ifdef CHANGED_3

#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "vm/pagepool.h"
#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "fs/sfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"
#include "lib/debug.h"


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

    uint32_t root_inode;

    // block allocation block count
    uint32_t bab_count;

    uint32_t block_count;

    // allocate here to save stack space
    union {
        char buffer[SFS_BLOCK_SIZE];
        bitmap_t bitmap;
    } bab;
    union {
        char buffer[SFS_BLOCK_SIZE];
        sfs_inode_t node;
    } inode;
    // only use stack reservations when not holding sfs->lock
} sfs_t;

#define SFS_BLOCKS_PER_BAB (SFS_BLOCK_SIZE * 8)

int sfs_read_block(sfs_t *sfs, uint32_t block, void *buffer) {
    gbd_request_t req;

    req.block = block;
    req.sem = NULL;
    req.buf = ADDR_KERNEL_TO_PHYS((uint32_t)buffer);
    return sfs->disk->read_block(sfs->disk, &req);
}

int sfs_is_block_free(sfs_t *sfs, uint32_t block) {
    int r, offset_in_bab;
    uint32_t bab_block;
    
    KERNEL_ASSERT(block < sfs->block_count);
    bab_block = block / SFS_BLOCKS_PER_BAB;
    KERNEL_ASSERT(bab_block < sfs->bab_count);
    
    // on disk the first block is magic block
    r = sfs_read_block(sfs, bab_block + 1, &(sfs->bab.buffer)); 
    KERNEL_ASSERT(r != 0);

    offset_in_bab = block - bab_block * SFS_BLOCKS_PER_BAB;

    kprintf("offset %d, block %d, bab_block %d\n", offset_in_bab, block, bab_block);
    return bitmap_get(&(sfs->bab.bitmap), offset_in_bab);
}

uint32_t sfs_root_inode(sfs_t *sfs) {
    return 1 + sfs->bab_count; 
}

/** 
 * Initialize trivial filesystem. Allocates 1 page of memory dynamically for
 * filesystem data structure, sfs data structure and buffers needed.
 * Sets fs_t and sfs_t fields. If initialization is succesful, returns
 * pointer to fs_t data structure. Else NULL pointer is returned.
 *
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

    sfs->disk = disk;
    sfs->lock = lock;
    sfs->block_count = disk->total_blocks(disk);
    sfs->bab_count = ((sfs->block_count - 1) + 1024)/1025;
    sfs->root_inode = sfs->bab_count + 1;

    DEBUG("sfsdebug", "SFS: Found %d BABs, %d total blocks\n", sfs->bab_count, sfs->block_count);

    // do some quick sanity checks:
    // - check that the first block is always marked as used (as it's reserved for root inode)
    if (sfs_is_block_free(sfs, 0) != 0) {
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
    sfs_t *sfs;
    sfs = (sfs_t*)fs->internal;

    lock_acquire(sfs->lock); 

    lock_destroy(sfs->lock); 
    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS((uint32_t)fs));
    return VFS_OK;
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
int sfs_open(fs_t *fs, char *filename)
{
    fs = fs;
    filename = filename;
    return VFS_ERROR;
}


/**
 * Closes file. Implements fs.close(). There is nothing to be done, no
 * data strucutures or similar are reserved for file. Returns VFS_OK.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param fileid File id (inode block number) of the file.
 *
 * @return VFS_OK
 */
int sfs_close(fs_t *fs, int fileid)
{
    fs = fs;
    fileid = fileid;
    return VFS_ERROR;
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
    uint32_t next_dir_inode;
    sfs_inode_dir_t *dir_inode;
     
    if ((uint32_t)size > SFS_MAX_FILESIZE) {
        return VFS_ERROR;
    }

    lock_acquire(sfs->lock);

    // check if the file exists or not
    next_dir_inode = sfs_root_inode(sfs); 
    while (next_dir_inode != 0) {
        r = sfs_read_block(sfs, next_dir_inode, &(sfs->inode.buffer));
        if (r == 0) {
            lock_release(sfs->lock);
            return VFS_ERROR;
        }
        if (sfs->inode.node.inode_type != SFS_DIR_INODE) {
            kprintf("SFS: inode should be dir but isn't! inode %d\n", next_dir_inode);
            KERNEL_PANIC("SFS: corrupted filesystem!\n");
        }
        dir_inode = &(sfs->inode.node.dir);
        for (i = 0; i < (int)SFS_ENTRIES_PER_DIR; i++) {
            if (dir_inode->entries[i].inode > 0 && stringcmp(dir_inode->entries[i].name, filename) == 0) {
                DEBUG("sfsdebug", "SFS sfs_create: File %s already exists!\n", filename);
                lock_release(sfs->lock);
                return VFS_ERROR;
            }
        } 
        next_dir_inode = dir_inode->next_dir_inode;
    }

    lock_release(sfs->lock);
    DEBUG("sfsdebug", "SFS sfs_create: ok, creating file %s\n", filename);

    return VFS_ERROR;
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
    fs = fs;
    filename = filename;
    return VFS_ERROR;
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
    fs = fs;
    fileid = fileid;
    buffer = buffer;
    bufsize = bufsize;
    offset = offset;
    return VFS_ERROR;
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
    fs = fs;
    fileid = fileid;
    buffer = buffer;
    datasize = datasize;
    offset = offset;
    return VFS_ERROR;
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
    fs = fs;
    return VFS_ERROR;
}

#endif

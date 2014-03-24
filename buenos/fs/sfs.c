#ifdef CHANGED_3

#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "vm/pagepool.h"
#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "fs/sfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"


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
} sfs_t;


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

    KERNEL_ASSERT(SFS_BLOCK_SIZE >= sizeof(sfs_inode_t));

    if (disk->block_size(disk) != SFS_BLOCK_SIZE)
        return NULL;

    addr = pagepool_get_phys_page();
    if (addr == 0) {
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
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        kprintf("sfs_init: Error during disk read. Initialization failed.\n");
        return NULL; 
    }

    if (buffer[0] != SFS_MAGIC) {
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        return NULL;
    }

    stringcopy(name, (((char*)buffer) + 4), SFS_VOLUMENAME_MAX);
    fs = (fs_t*)addr;
    sfs = (sfs_t*)(addr + sizeof(fs_t));

    sfs->disk = disk;
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
    fs = fs;
    return VFS_ERROR;
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
    fs = fs;
    filename = filename;
    size = size;
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

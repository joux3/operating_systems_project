#ifdef CHANGED_3

#ifndef FS_SFS_H
#define FS_SFS_H

#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"

/* In SFS block size is 128 */
#define SFS_BLOCK_SIZE 128

/* Magic number found on each sfs filesystem's header block. */
#define SFS_MAGIC 1337

/* Max concurrent readers permitted to jile access */
#define SFS_MAX_READERS 64
#define SFS_MAX_OPEN_FILES 64

/* Names are limited to 16 characters */
#define SFS_VOLUMENAME_MAX 16
#define SFS_FILENAME_MAX 16

/*
    max direct data blocks in file inode
*/
#define SFS_DIRECT_DATA_BLOCKS ((SFS_BLOCK_SIZE - 5*sizeof(uint32_t)) / sizeof(uint32_t))

#define SFS_INDIRECT_POINTERS (SFS_BLOCK_SIZE / sizeof(uint32_t))

/* Maximum file size */
#define SFS_MAX_FILESIZE (SFS_DIRECT_DATA_BLOCKS * SFS_BLOCK_SIZE +\
                          SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE +\
                          SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE +\
                          SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE)

#define SFS_SECOND_INDIRECT_SIZE (SFS_DIRECT_DATA_BLOCKS * SFS_BLOCK_SIZE +\
                          SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE +\
                          SFS_INDIRECT_POINTERS * SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE)
#define SFS_FIRST_INDIRECT_SIZE (SFS_DIRECT_DATA_BLOCKS * SFS_BLOCK_SIZE +\
                          SFS_INDIRECT_POINTERS * SFS_BLOCK_SIZE)
#define SFS_DIRECT_SIZE (SFS_DIRECT_DATA_BLOCKS * SFS_BLOCK_SIZE)

#define SFS_FILE_INODE 128
#define SFS_DIR_INODE 918

typedef struct {
    uint32_t filesize;
    uint32_t direct_blocks[SFS_DIRECT_DATA_BLOCKS];
    uint32_t first_indirect;
    uint32_t second_indirect;
    uint32_t third_indirect;
} sfs_inode_file_t;

typedef struct {
    uint32_t inode;
    char name[SFS_FILENAME_MAX];
} sfs_dir_entry_t;

#define SFS_ENTRIES_PER_DIR ((SFS_BLOCK_SIZE - 2*sizeof(uint32_t)) / sizeof(sfs_dir_entry_t))

typedef struct {
    uint32_t next_dir_inode; // 0 when no next
    sfs_dir_entry_t entries[];
} sfs_inode_dir_t;

typedef struct {
    uint32_t inode_type;

    union {
        sfs_inode_file_t file;
        sfs_inode_dir_t dir;
    };
} sfs_inode_t;


/* functions */
fs_t * sfs_init(gbd_t *disk);

int sfs_unmount(fs_t *fs);
int sfs_open(fs_t *fs, char *filename);
int sfs_close(fs_t *fs, int fileid);
int sfs_create(fs_t *fs, char *filename, int size);
int sfs_remove(fs_t *fs, char *filename);
int sfs_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset);
int sfs_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset);
int sfs_getfree(fs_t *fs);


#endif    /* FS_SFS_H */

#endif

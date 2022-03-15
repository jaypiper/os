#ifndef OS_VFS_H
#define OS_VFS_H

#define MB << 20
#define KB << 10

/* configs */
#define FS_START (1 MB)
#define BLK_SIZE (1 KB)
#define FS_BLK (FS_START / BLK_SIZE)
#define LOG_SIZE (BLK_SIZE - sizeof(superblock_t))
#define N_SUPER (FS_START / BLK_SIZE)
#define N_LOG ((BLK_SIZE - sizeof(superblock_t)) / sizeof(fslog_t))
#define N_INODE 128  // less than 2^16
#define INODE_PER_BLOCK (BLK_SIZE / sizeof(inode_t))
#define INODE_BLK_NUM ((N_INODE - 1) / INODE_PER_BLOCK + 1)
#define DIREN_PER_BLOCK (BLK_SIZE / sizeof(diren_t))
#define MAX_DIRECT_FILE_BLOCK 8
#define SUPER_MAGIC 0xabcd1234

#define SUPER_BLOCK_ADDR FS_START
#define LOG_ADDR (SUPER_BLOCK_ADDR + sizeof(superblock_t))
#define INODE_BASE (FS_START + BLK_SIZE)
#define INODE_BLK_IDX(idx) ((idx) / INODE_PER_BLOCK)
#define INODE_BLK_OFFSET(idx) (((idx) % INODE_PER_BLOCK) * sizeof(inode_t))
#define INODE_ADDR(idx) (INODE_BASE + INODE_BLK_IDX(idx) * BLK_SIZE + INODE_BLK_OFFSET(idx))

#define FREE_BITMAP_ADDR (INODE_BASE + (INODE_BLK_NUM * BLK_SIZE))

#define BLK2ADDR(idx) (((uintptr_t)idx) * BLK_SIZE)
#define INDIRECT_NUM_PER_BLK (BLK_SIZE / sizeof(uint32_t) - 1)
#define DIREN_NAME_LEN 31

#define UP_BLK_NUM(idx, num_per_blk) (((idx) + (num_per_blk) - 1) / (num_per_blk))

enum{FT_FILE = 0, FT_DIR};
// entry type [single | start [mid...] end]
enum{DIRENT_END = 0, DIRENT_MID, DIRENT_START, DIRENT_SINGLE};

typedef struct superblock{
    uint32_t super_magic;
    uint32_t fssize; // 4 GB at most
    uint32_t n_blk;
    uint32_t n_inode;
    uint32_t n_log;
    uint32_t used_blk;
    uint32_t inode_start;
    uint32_t bitmap_start;
    uint32_t data_start;
} superblock_t;
/* name of the file/dir corresponding to the inode is stored in the dir entry of its parent */
#define DEPTH_IN_INODE MAX_DIRECT_FILE_BLOCK
#define INDIRECT_IN_INODE (MAX_DIRECT_FILE_BLOCK + 1)
typedef struct inode{
    int type;
    int n_link;
    uint32_t size;
    uint32_t addr[MAX_DIRECT_FILE_BLOCK + 2];  // blk idx: direct(12), depth(1), next_block(1)
} inode_t;

typedef struct fslog{
    int type;
    uint32_t val;
} fslog_t;

typedef struct diren{
    union{
        uint32_t inode_idx;
        uint32_t next_entry;   // inode is stored in the last entry of file
    };
    char name[DIREN_NAME_LEN];
    uint8_t type;
}diren_t;

#define MAX32bit 0xffffffff

#endif

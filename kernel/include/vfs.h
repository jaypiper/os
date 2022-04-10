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
#define N_INODE 512  // less than 2^16
#define INODE_BLK_NUM ((N_INODE * sizeof(inode_t) - 1) / BLK_SIZE + 1)
#define DIREN_PER_BLOCK (BLK_SIZE / sizeof(diren_t))
#define MAX_DIRECT_FILE_BLOCK 8
#define MAX_OPEN_FILE 32
#define SUPER_MAGIC 0xabcd1234

#define SUPER_BLOCK_ADDR FS_START
#define LOG_ADDR (SUPER_BLOCK_ADDR + sizeof(superblock_t))
#define INODE_BASE (FS_START + BLK_SIZE)
#define INODE_ADDR(idx) (INODE_BASE + idx * sizeof(inode_t))

#define FREE_BITMAP_ADDR (INODE_BASE + (INODE_BLK_NUM * BLK_SIZE))

#define BLK2ADDR(idx) (((uintptr_t)idx) * BLK_SIZE)
#define INDIRECT_NUM_PER_BLK (BLK_SIZE / sizeof(uint32_t) - 1)
#define DIREN_NAME_LEN 27

#define UP_BLK_NUM(idx, num_per_blk) (((idx) + (num_per_blk) - 1) / (num_per_blk))

#define OFFSET_IN_STRUCT(_struct, _member) ((uintptr_t)&_struct._member - (uintptr_t)&_struct)
#define OFFSET_IN_PSTRUCT(p_struct, _member) ((uintptr_t)&p_struct->_member - (uintptr_t)p_struct)

enum{FT_UNUSED = 0, FT_FILE , FT_DIR, FT_LINK, FT_PROC_DIR, FT_PROC_FILE};
// entry type [single | start [mid...] end]
enum{DIRENT_INVALID = 0, DIRENT_END, DIRENT_MID, DIRENT_START, DIRENT_SINGLE};
enum{CWD_INVALID = 0, CWD_UFS, CWD_PROCFS, CWD_DEVFS};

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
    union{
        uint32_t addr[MAX_DIRECT_FILE_BLOCK + 2];  // blk idx: direct(12), depth(1), next_block(1)
        int link_no; // for link file
    };
} inode_t;

typedef struct fslog{
    int type;
    uint32_t val;
} fslog_t;

typedef struct diren{
    union{
        int inode_idx;
        int next_entry;   // inode is stored in the last entry of file
    };
    char name[DIREN_NAME_LEN];
    uint8_t type;
}diren_t;

#define PROC_NAME_LEN (32 - sizeof(uintptr_t))

typedef struct proc_inode{
    int type;
    int size;
    void* mem;
}proc_inode_t;

typedef struct proc_diren{
    proc_inode_t* inode;
    char name[PROC_NAME_LEN];
}proc_diren_t;

#define DEV_NAME_LEN 16
#define MAX_DEV_NUM 16

typedef struct ofile_info ofile_info_t;

typedef struct dev_inode{
    int size;
    char name[16];
    int (*write)(struct ofile_info* ofile, int fd, void *buf, int count);
    int (*read)(struct ofile_info* ofile, int fd, void *buf, int count);
    int (*lseek)(struct ofile_info* ofile, int fd, int offset, int whence);
}dev_inode_t;

#ifndef IN_MKFS

typedef struct ofile_info{
    int (*write)(struct ofile_info* ofile, int fd, void *buf, int count);
    int (*read)(struct ofile_info* ofile, int fd, void *buf, int count);
    int (*lseek)(struct ofile_info* ofile, int fd, int offset, int whence);
    int offset;
    union{
        int inode_no;
        proc_inode_t* proc_inode;
        dev_inode_t* dev_inode;
    };
    int type;       // ufs, proc, dev
    int flag;
    int count;
    sem_t lock;
}ofile_info_t;

#endif

#define MAX32bit 0xffffffff
#define ROOT_INODE_NO 0

/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */

#define IS_VALID_FD(fd) ((fd >= 0) && (fd < MAX_OPEN_FILE))

int dev_input_read(ofile_info_t* ofile, int fd, void *buf, int count);
int dev_output_write(ofile_info_t* ofile, int fd, void *buf, int count);
int dev_error_write(ofile_info_t* ofile, int fd, void *buf, int count);

void vfs_readFileList(int root_idx, int depth);
void new_proc_init(int id, const char* name);
void delete_proc(int pid);
void modify_proc_info(int pid, char* file_name, void* data, int sz);
#ifndef IN_MKFS
void fill_standard_fd(task_t* task);
#endif

int invalid_write(ofile_info_t* ofile, int fd, void *buf, int count);
int invalid_read(ofile_info_t* ofile, int fd, void *buf, int count);
int invalid_lseek(ofile_info_t* ofile, int fd, int offset, int whence);

ofile_info_t* filedup(ofile_info_t* ofile);
void fileclose(ofile_info_t* ofile);

#endif

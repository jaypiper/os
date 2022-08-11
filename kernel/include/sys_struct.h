#ifndef SYS_STRUCT
#define SYS_STRUCT

typedef long time_t;

typedef struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
}utsname;

typedef struct winsize{
    unsigned short int ws_row;
    unsigned short int ws_col;
    unsigned short int ws_xpixel;
    unsigned short int ws_ypixel;
} winsize;

typedef struct iovec {
    void *iov_base;	/* Pointer to data.  */
    uint64_t iov_len;	/* Length of data.  */
}iovec;

typedef struct statfs {
    int64_t f_type;
    int64_t f_bsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    int64_t f_files;
    int64_t f_ffree;
    int f_fsid[2];
    int64_t f_namelen;
    int64_t f_frsize;
    int64_t f_flags;
    int64_t f_spare[4];
}statfs;

typedef struct stat {
	uint64_t st_dev;
	uint64_t st_ino;
	uint32_t st_mode;
	int32_t st_nlink;
	uint32_t st_uid;
	uint32_t st_gid;
	uint64_t st_rdev;
	unsigned long __pad;
	int64_t st_size;
	int64_t st_blksize;
	int __pad2;
	int64_t st_blocks;
	time_t st_atim_sec;
    long st_atim_nsec;
    time_t st_mtim_sec;
    long st_mtim_nsec;
    time_t st_ctim_sec;
    long st_ctim_nsec;
	uint32_t unused[2];
}stat;

#define SYSLOG_ACTION_READ_ALL 3
#define SYSLOG_ACTION_SIZE_BUFFER 10

#endif

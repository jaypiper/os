#ifndef SYS_STRUCT
#define SYS_STRUCT

typedef long time_t;
// typedef uint64_t ino_t;
// typedef int64_t off_t;

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

typedef struct linux_dirent {
	uint64_t d_ino;
	int64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[64];
}linux_dirent;

typedef struct timeval_t{
    long tv_sec;		/* Seconds.  */
    long tv_usec;	/* Microseconds.  */
}timeval_t;

typedef struct rusage {
    long ru_utime_sec; /* user CPU time used */
    long ru_utime_usec;
    long ru_stime_sec; /* system CPU time used */
    long ru_stime_usec;
    long   ru_maxrss;        /* maximum resident set size */
    long   ru_ixrss;         /* integral shared memory size */
    long   ru_idrss;         /* integral unshared data size */
    long   ru_isrss;         /* integral unshared stack size */
    long   ru_minflt;        /* page reclaims (soft page faults) */
    long   ru_majflt;        /* page faults (hard page faults) */
    long   ru_nswap;         /* swaps */
    long   ru_inblock;       /* block input operations */
    long   ru_oublock;       /* block output operations */
    long   ru_msgsnd;        /* IPC messages sent */
    long   ru_msgrcv;        /* IPC messages received */
    long   ru_nsignals;      /* signals received */
    long   ru_nvcsw;         /* voluntary context switches */
    long   ru_nivcsw;        /* involuntary context switches */
}rusage_t;

// d_type
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14


#define SYSLOG_ACTION_READ_ALL 3
#define SYSLOG_ACTION_SIZE_BUFFER 10

/* fcntl */
#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7
#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11

#endif

#ifndef SYS_STRUCT
#define SYS_STRUCT

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
};

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

#endif

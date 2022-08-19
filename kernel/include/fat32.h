#ifndef FAT32_H
#define FAT32_H

#define ATTR_READ_ONLY  0x1     // Indicates that writes to the file should fail
#define ATTR_HIDDEN     0x2     // Indicates that normal directory listings should not show this file. 
#define ATTR_SYSTEM     0x4     // Indicates that this is an operating system file.
#define ATTR_VOLUME_ID  0x8     /* There should only be one “file” on the volume that has this attribute
                                  set, and that file must be in the root directory. This name of this file is
                                  actually the label for the volume. DIR_FstClusHI and
                                  DIR_FstClusLO must always be 0 for the volume label (no data
                                  clusters are allocated to the volume label file). */
#define ATTR_LONG_NAME  0xf     // indicates that the “file” is actually part of the long name entry for some other file
#define ATTR_DIRECTORY  0x10    // Indicates that this file is actually a container for other files.
#define ATTR_ARCHIVE    0x20    /* This attribute supports backup utilities. This bit is set by the FAT file
                                  system driver when a file is created, renamed, or written to. Backup
                                  utilities may use this attribute to indicate which files on the volume
                                  have been modified since the last time that a backup was performed. */

#define FAT32_EOF 0x0ffffff8
#define LAST_LONG_ENTRY 0x40
#define ENTRY_EMPTY_LAST 0
#define ENTRY_EMPTY 0xe5
#define AT_FDCWD (-100)

#define FAT32_MAX_PATH_LENGTH 64
#define MAX_OPEN_FILE 32
#define IS_VALID_FD(fd) ((fd >= 0) && (fd < MAX_OPEN_FILE))

#define LONG_NAME_LENGTH 13
#define SHORT_NAME_LANGTH 11

/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */

#define AT_REMOVEDIR 0x200

#define PROC_SUPER_MAGIC      0x9fa0
#define TMPFS_MAGIC           0x01021994

// Boot Sector
typedef struct BS_Structure{
          // uint8_t   BS_jmpBoot[3];
          // uint8_t   BS_OEMName[8];
          uint16_t  BPB_BytsPerSec;
          uint8_t   BPB_SecPerClus;
          uint16_t  BPB_RsvdSecCnt;
/* 16 */  uint8_t   BPB_NumFATs;
          uint16_t  BPB_RootEntCnt;    // 512 for FAT12/16, 0 for FAT32
          // uint16_t  BPB_TotSec16;       // 0 (>32MB)
          // uint8_t   BPB_Media;
          // uint16_t  BPB_FATSz16;          // 0 for FAT32
// /* 24 */  uint16_t  BPB_SecPerTrk;
//           uint16_t  BPB_NumHeads;
          uint32_t  BPB_HiddSec;
/* 32 */  uint32_t  BPB_TotSec32;
          uint32_t  BPB_FATSz32;          // count of sectors
// /* 40 */  uint16_t  BPB_ExtFlags;
//           uint16_t  BPB_FSVer;
          uint32_t  BPB_RootClus;        //root dir
/* 48 */  uint16_t  BPB_FSInfo;
          uint16_t  BPB_BkBootSec;
          uint8_t   BPB_Reserved[12];
// /* 64 */  uint8_t   BS_DrvNum;
//           uint8_t   BS_Reserved1;
//           uint8_t   BS_BootSig;
//           uint32_t  BS_VolID;
//           uint8_t   BS_VolLab[11];    // ASCII
//           uint8_t   BS_FilSysType[8];          // ASCII, "FAT32"
// /* 90 */  uint8_t   reserved3[420];
//           uint16_t  endMagic;           //0xaa55

  /* for OS usage */
            uint32_t BytePerClus;
            uint32_t RsvByte;
            uint32_t DataStartByte;
}FAT32_BS;

typedef struct dirent{
  uint8_t   name[FAT32_MAX_PATH_LENGTH - 1];

  uint8_t   attr;
  uint32_t  FstClus;
  uint32_t FileSz;

  int ref;
  struct dirent* parent;
  uint32_t offset;      // sd offset in parent
  uint32_t clus_in_parent;
  uint64_t ent_num;
}dirent_t;

typedef struct fat32_sdirent{
  uint8_t   name[11];
  uint8_t   attr;
  uint8_t   NTRes;
  uint8_t   CrtTimeTenth;
  uint16_t  CrtTime;
  uint16_t  CrtDate;
  uint16_t  LstAccDate;
  uint16_t  FstClusHi;
  uint16_t  WrtTime;
  uint16_t  WrtDate;
  uint16_t  FstClusLo;
  uint32_t  FileSz;
}sdirent_t;

typedef struct fat32_ldirent{
  uint8_t Ord;
  uint8_t name1[10];
  uint8_t attr;
  uint8_t type;
  uint8_t Chksum;
  uint8_t name2[12];
  uint16_t FstClusLo;
  uint8_t name3[4];
}ldirent_t;

typedef struct fat32_dirent{
  union{
    sdirent_t sd;
    ldirent_t ld;
  };
}fat32_dirent_t;

typedef struct builtin_dirent bdirent_t;

typedef struct pipe_t{
  void* buf;
  int w_ptr;
  int buf_size;
  int r_ptr;
}pipe_t;

typedef struct ofile_info{
    int (*write)(struct ofile_info* ofile, int fd, void *buf, int count);
    int (*read)(struct ofile_info* ofile, int fd, void *buf, int count);
    int (*lseek)(struct ofile_info* ofile, int fd, int offset, int whence);
    int offset;
    union{
      dirent_t* dirent;
      bdirent_t* bdirent;
      pipe_t* pipe;
    };
    int type;       // ufs, proc, dev
    int flag;
    int count;
    sem_t lock;
}ofile_t;

void init_task_cwd(task_t* task);
dirent_t* dup_dirent(dirent_t * dirent);
ofile_t* filedup(ofile_t* ofile);
void fileclose(ofile_t* ofile);

#define CWD_FAT 1
#define CWD_BFS 2
#define CWD_PIPEOUT 3
#define CWD_PIPEIN 4

#endif

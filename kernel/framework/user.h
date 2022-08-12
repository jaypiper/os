#include <stdint.h>

#define T_DIR     1
#define T_FILE    2

#define SEEK_CUR  0
#define SEEK_SET  1
#define SEEK_END  2

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_APPEND  0x004
// #define O_CREAT   0x200
#define O_CREAT   0x40

#define PROT_READ        0x1                /* Page can be read.  */
#define PROT_WRITE        0x2                /* Page can be written.  */
#define PROT_EXEC        0x4                /* Page can be executed.  */
#define PROT_NONE        0x0                /* Page can not be accessed.  */
#define MAP_ANONYMOUS        0x20                /* Don't use a file.  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED        0x01                /* Share changes.  */
#define MAP_PRIVATE        0x02                /* Changes are private.  */

struct ufs_stat {
  uint32_t id, type, size;
};

typedef unsigned int mode_t;
typedef long int off_t;

typedef struct kstat {
  uint64_t st_dev;
  uint64_t st_ino;
  mode_t st_mode;
  uint32_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  uint64_t st_rdev;
  unsigned long __pad;
  off_t st_size;
  uint32_t st_blksize;
  int __pad2;
  uint64_t st_blocks;
  long st_atime_sec;
  long st_atime_nsec;
  long st_mtime_sec;
  long st_mtime_nsec;
  long st_ctime_sec;
  long st_ctime_nsec;
  unsigned unused[2];
}kstat_t;

#define S_IFREG 0100000

struct ufs_dirent {
  uint32_t inode;
  char name[28];
} __attribute__((packed));

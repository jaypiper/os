#include <stdint.h>

#define T_DIR     1
#define T_FILE    2

#define SEEK_CUR  0
#define SEEK_SET  1
#define SEEK_END  2

#define O_RDONLY  00000000
#define O_WRONLY  00000001
#define O_RDWR    00000002
#define O_CREAT   00000100

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

struct ufs_dirent {
  uint32_t inode;
  char name[28];
} __attribute__((packed));

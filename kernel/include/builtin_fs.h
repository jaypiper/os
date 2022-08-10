#ifndef BUILTIN_FS
#define BUILTIN_FS

#include <common.h>
#define MAX_BUILTIN_NAME 31
#define MAX_BNETRY_PER_PAGE (PGSIZE / sizeof(bdirent_t) - 1)
#define DIRECT_NUM 3
#define INDIRECT_PER_PAGE (PGSIZE / sizeof(uintptr_t) - 1)
enum {BD_DIR, BD_FILE, BD_EMPTY};

typedef struct builtin_dirent{
    uint8_t name[MAX_BUILTIN_NAME];
    uint8_t type;
    uint32_t size;
    uintptr_t direct_addr[DIRECT_NUM];
    uintptr_t indirent_addr;
    uint8_t dummy[4];
}bdirent_t;

int bfs_read(ofile_t* ofile, int fd, void *buf, int count);
int bfs_write(ofile_t* ofile, int fd, void *buf, int count);
#endif
#include <common.h>
#include <fat32.h>
#include <builtin_fs.h>

spinlock_t bfs_lock;

uintptr_t get_bfs_pg(int pg_idx, bdirent_t* bdirent){
    if(pg_idx < 3) return bdirent->direct_addr[pg_idx];
    int depth = (pg_idx - DIRECT_NUM) / INDIRECT_PER_PAGE;
    int offset = (pg_idx - DIRECT_NUM) % INDIRECT_PER_PAGE;
    uintptr_t addr = bdirent->indirent_addr;
    for(int i = 0; i < depth; i++){
        addr = ((uintptr_t*)addr)[INDIRECT_PER_PAGE];
    }
    return ((uintptr_t*)addr)[offset];
}

uintptr_t new_bfs_pg(bdirent_t* bdirent){
    int num = ROUNDUP(bdirent->size, PGSIZE) / PGSIZE;
    if(num < 3){
        bdirent->direct_addr[num] = pgalloc(PGSIZE);
        return bdirent->direct_addr[num];
    }
    uintptr_t ret = pgalloc(PGSIZE);
    if(num == 3) bdirent->indirent_addr = pgalloc(PGSIZE);
    int depth = (num - DIRECT_NUM) / INDIRECT_PER_PAGE;
    int offset = (num - DIRECT_NUM) % INDIRECT_PER_PAGE;
    uintptr_t* addr = bdirent->indirent_addr;
    for(int i = 0; i < depth; i++){
        addr = addr[INDIRECT_PER_PAGE];
    }
    uintptr_t* next_addr = addr + 1;
    if(offset == (INDIRECT_PER_PAGE - 1)){
        uintptr_t* next_indirect_page = pgalloc(PGSIZE);
        *next_addr = next_indirect_page;
        *next_indirect_page = ret;
    } else{
        *next_addr = ret;
    }
    return ret;
}

bdirent_t* bfs_empty(bdirent_t* bfs){
    int num = bfs->size / sizeof(bdirent_t);
    int pg_idx = num / MAX_BNETRY_PER_PAGE;
    int idx = num % MAX_BNETRY_PER_PAGE;
    uintptr_t pg_addr = get_bfs_pg(pg_idx, bfs);
    bdirent_t* bdirent = (bdirent_t*)pg_addr + idx;
    bfs->size += sizeof(bdirent_t);
    if(idx == (MAX_BNETRY_PER_PAGE - 1)){
        new_bfs_pg(bfs);
    }

    return bdirent;
}

void insert_bfs_entry(bdirent_t* parent, char* name, char* content){
    bdirent_t* child = bfs_empty(parent);
    strcpy((char*)child->name, name);
    child->type = BD_FILE;
    child->size = 0;
    int size = strlen(content);
    while(size){
        uintptr_t pg_addr = new_bfs_pg(child);
        int copy_size = MIN(PGSIZE, size);
        memcpy(pg_addr, content, copy_size);
        size -= copy_size;
        content += copy_size;
        child->size += copy_size;
    }
}

void tmpfs_init(bdirent_t* root_bfs){
    bdirent_t* bdirent = bfs_empty(root_bfs);
    strcpy((char*)bdirent->name, "tmp");
    bdirent->type = BD_DIR;
    bdirent->size = 0;
    bdirent->direct_addr[0] = pgalloc(PGSIZE);
}

void etc_init(bdirent_t* root_bfs){
    bdirent_t* etc = bfs_empty(root_bfs);
    strcpy((char*)etc->name, "etc");
    etc->type = BD_DIR;
    etc->size = 0;
    etc->direct_addr[0] = pgalloc(PGSIZE);
    // insert_bfs_entry(etc, "localtime", )
}

void sbin_init(bdirent_t* root_bfs){
    bdirent_t* sbin = bfs_empty(root_bfs);
    strcpy((char*)sbin->name, "sbin");
    sbin->type = BD_DIR;
    sbin->size = 0;
    sbin->direct_addr[0] = pgalloc(PGSIZE);
    insert_bfs_entry(sbin, "ls", "");
}

void proc_init(bdirent_t* root_bfs){
    bdirent_t* proc = bfs_empty(root_bfs);
    strcpy((char*)proc->name, "proc");
    proc->type = BD_DIR;
    proc->size = 0;
    proc->direct_addr[0] = pgalloc(PGSIZE);
    insert_bfs_entry(proc, "mounts", \
        "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n\
        tmpfs /tmp tmpfs rw,nosuid,nodev,relatime,size=4096k,nr_inodes=10,mode=700");
}


void bfs_init(bdirent_t* root_bfs){
    strcpy(root_bfs->name, "/");
    root_bfs->type = BD_DIR;
    root_bfs->size = 0;
    root_bfs->direct_addr[0] = pgalloc(PGSIZE);
    tmpfs_init(root_bfs);
    proc_init(root_bfs);
    sbin_init(root_bfs);
    spin_init(&bfs_lock, "bfs_lock");
}

bdirent_t* search_in_bfs(bdirent_t* dir, char* name){
    int pg_num = 0;
    int pg_offset = 0;
    bdirent_t* bdirent = dir->direct_addr[0];
    bdirent_t* ret = NULL;
    int num = dir->size / sizeof(bdirent_t);
    while(pg_offset < num){
        bdirent_t* select = bdirent + pg_offset;
        if(!strcmp(select->name, name)){
            ret = select;
            break;
        }
        pg_offset ++;
        if(pg_offset == MAX_BNETRY_PER_PAGE){
            pg_offset = 0;
            pg_num += 1;
            bdirent = get_bfs_pg(pg_num, dir);
            num -= PGSIZE / sizeof(bdirent_t);
        }
    }
    return ret;
}

bdirent_t* create_in_bfs(bdirent_t* dir, char* filename, int flags){
    bdirent_t* bdirent = bfs_empty(dir);
    strcpy(bdirent->name, filename);
    bdirent->size = 0;
    if(flags & ATTR_DIRECTORY){
        bdirent->type = BD_DIR;
        bdirent->direct_addr[0] = pgalloc(PGSIZE);
    } else {
        bdirent->type = BD_FILE;
    }
    return bdirent;
}

bdirent_t* bfs_search(bdirent_t* dir, char* path){
    char* token = path;
    int delim_idx = find_replace(path, "/", 0);
    while(token) {
        if(!dir || (dir->type != BD_DIR)) return NULL;
        dir = search_in_bfs(dir, token);

        if(delim_idx == -1) break;
        path += delim_idx + 1;
        token = path;
        delim_idx = find_replace(path, "/", 0);
    }
    return dir;
}

bdirent_t* bfs_create(bdirent_t* dir, char* path, int flags){
    int name_idx = split_base_name(path);
    char* filename = path;
    if(name_idx != -1){
        dir = bfs_search(dir, path);
        filename =  path + name_idx + 1;
    }
    if(!dir) return NULL;
    bdirent_t* bdirent = search_in_bfs(dir,filename);
    if(bdirent) return bdirent;
    return create_in_bfs(dir, filename, flags);
}

int bfs_unlink(bdirent_t* dir, char* path, int is_dir){ // not in bfs(-1) in bfs(0)
    char* token = path;
    int delim_idx = find_replace(path, "/", 0);
    bdirent_t* parent = dir;
    while(token) {
        if(!dir || (dir->type != BD_DIR)) return -1;
        parent = dir;
        dir = search_in_bfs(dir, token);

        if(delim_idx == -1) break;
        path += delim_idx + 1;
        token = path;
        delim_idx = find_replace(path, "/", 0);
    }
    if(!dir) return -1;

    if(dir->type == BD_DIR && !is_dir) return 0;
    if(dir->type == BD_FILE && is_dir) return 0;
    int pg_num = (parent->size - sizeof(bdirent_t)) / PGSIZE;
    int pg_offset = (parent->size - sizeof(bdirent_t)) % PGSIZE;
    uintptr_t pg_addr = get_bfs_pg(pg_num, parent);
    bdirent_t* last = (bdirent_t*)(pg_addr + pg_offset);
    memcpy(dir, last, sizeof(bdirent_t));
    last->type = BD_EMPTY;
    dir->size -= sizeof(bdirent_t);
    return 0;
}

int bfs_read(ofile_t* ofile, int fd, void *buf, int count){
    if(ofile->offset > ofile->bdirent->size){
        TODO();
    }
    uintptr_t pg_idx = ofile->offset / PGSIZE;
    bdirent_t* bdirent = ofile->bdirent;
    count = MIN(count, bdirent->size - ofile->offset);
    uintptr_t pg_offset = ofile->offset % PGSIZE;
    uintptr_t readsize = count;
    while(count){
        uintptr_t pg_ptr = get_bfs_pg(pg_idx, bdirent);
        uintptr_t size = MIN(PGSIZE - pg_offset, count);
        memcpy(buf, pg_ptr + pg_offset, size);
        pg_offset = 0;
        pg_idx ++;
        count -= size;
        buf += size;
    }
    ofile->offset += readsize;
    return readsize;
}

int bfs_write(ofile_t* ofile, int fd, void *buf, int count){
    if(ofile->offset > ofile->bdirent->size){
        TODO();
    }
    uintptr_t pg_idx = ofile->offset / PGSIZE;
    bdirent_t* bdirent = ofile->bdirent;
    uintptr_t pg_offset = ofile->offset % PGSIZE;
    uintptr_t writesize = count;
    uintptr_t offset = ofile->offset;
    while(count){
        if(offset > bdirent->size) new_bfs_pg(bdirent);
        uintptr_t pg_ptr = get_bfs_pg(pg_idx, bdirent);
        uintptr_t size = MIN(PGSIZE - pg_offset, count);
        memcpy(pg_ptr + pg_offset, buf, size);
        pg_offset = 0;
        pg_idx ++;
        count -= size;
        buf += size;
        offset += size;
    }
    bdirent->size = MAX(bdirent->size, ofile->offset + writesize);
    ofile->offset += writesize;
    return writesize;
}

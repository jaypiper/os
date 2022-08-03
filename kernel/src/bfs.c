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
    return addr + offset * sizeof(uintptr_t);
}

uintptr_t new_bfs_pg(bdirent_t* bdirent){
    int num = bdirent->size / PGSIZE;
    if(num < 3){
        bdirent->direct_addr[num] = pgalloc(PGSIZE);
        return bdirent->direct_addr[num];
    }
    uintptr_t ret = pgalloc(PGSIZE);
    if(num == 3) bdirent->indirent_addr = pgalloc(PGSIZE);
    uintptr_t* pg_ptr = get_bfs_pg(num, bdirent);

    int tmp_idx = ((uintptr_t)pg_ptr - (uintptr_t)ROUNDDOWN(pg_ptr, PGSIZE)) / sizeof(uintptr_t);
    uintptr_t* next_addr = pg_ptr + 1;
    if(tmp_idx == (INDIRECT_PER_PAGE - 1)){
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
    uintptr_t* pg_addr = (uintptr_t*)get_bfs_pg(pg_idx, bfs);
    bdirent_t* bdirent = pg_addr + idx;
    bfs->size += sizeof(bdirent_t);
    if(idx == (MAX_BNETRY_PER_PAGE - 1)){
        new_bfs_pg(bfs);
    }

    return bdirent;
}

void tmpfs_init(bdirent_t* root_bfs){
    bdirent_t* bdirent = bfs_empty(root_bfs);
    strcpy((char*)bdirent->name, "tmp");
    bdirent->type = BD_DIR;
    bdirent->size = 0;
    bdirent->offset = 0;
    bdirent->direct_addr[0] = pgalloc(PGSIZE);
    bdirent->parent = root_bfs;
}

void bfs_init(bdirent_t* root_bfs){
    strcpy(root_bfs->name, "/");
    root_bfs->type = BD_DIR;
    root_bfs->size = 0;
    root_bfs->offset = 0;
    root_bfs->direct_addr[0] = pgalloc(PGSIZE);
    tmpfs_init(root_bfs);
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
            bdirent = ((uintptr_t*)get_bfs_pg(pg_num, dir))[0];
        }
    }
    return ret;
}

bdirent_t* create_in_bfs(bdirent_t* dir, char* filename, int flags){
    bdirent_t* bdirent = bfs_empty(dir);
    strcpy(bdirent->name, filename);
    bdirent->size = 0;
    bdirent->offset = 0;
    bdirent->parent = dir;
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

int bfs_read(ofile_t* ofile, int fd, void *buf, int count){
    if(ofile->offset > ofile->bdirent->size){
        TODO();
    }
    uintptr_t pg_idx = ofile->offset / PGSIZE;
    bdirent_t* bdirent = ofile->bdirent;
    count = MIN(count, bdirent->size - bdirent->offset);
    uintptr_t pg_offset = ofile->offset % PGSIZE;
    uintptr_t readsize = count;
    while(count){
        uintptr_t pg_ptr = get_bfs_pg(pg_idx, bdirent);
        uintptr_t size = MIN(PGSIZE - pg_offset, count);
        memcpy(buf, *(uintptr_t*)pg_ptr + pg_offset, size);
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
        memcpy(*(uintptr_t*)pg_ptr + pg_offset, buf, size);
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
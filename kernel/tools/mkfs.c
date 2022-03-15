#include <user.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vfs.h>
#include <util.h>

#define MKFS_DEBUG

#include <debug.h>

static uint8_t *disk;

#define IN_DISK(offset) ((void*)(disk + offset))

/*
* boot, mainargs, kernel | super block | log | inode block | free bitmap | data block
*                      FS_START
*                        | <-  a block     ->|
*/

static int alloc_block(){
  superblock_t* sb = (superblock_t*)IN_DISK(SUPER_BLOCK_ADDR);
  Assert(sb->used_blk < sb->n_blk, "no avaliable blk, used %d/%d", sb->used_blk, sb->n_blk);
  uint32_t* bitmap = IN_DISK(BLK2ADDR(sb->bitmap_start));
  uint32_t blk_idx = 0, bitmap_idx = 0;
  for(blk_idx = 0; blk_idx < sb->n_blk; blk_idx += 32, bitmap_idx ++){
    if(bitmap[bitmap_idx] == MAX32bit) continue;
    else{
      int free_bit = __builtin_ctz(~bitmap[bitmap_idx]);
      blk_idx += free_bit;
      bitmap[bitmap_idx] = bitmap[bitmap_idx] | ((uint32_t)1 << free_bit);
      break;
    }
  }
  return sb->data_start + blk_idx;
}

static void insert_blk_into_inode(inode_t * inode, int blk_idx){
  int blk_num = UP_BLK_NUM(inode->size, BLK_SIZE);
  if(blk_num < MAX_DIRECT_FILE_BLOCK){
    inode->addr[blk_num] = blk_idx;
  }else{
    int idx = blk_idx - MAX_DIRECT_FILE_BLOCK;
    uint32_t* blk_start = IN_DISK(BLK2ADDR(inode->addr[INDIRECT_IN_INODE]));
    int depth = UP_BLK_NUM(idx, INDIRECT_NUM_PER_BLK);
    for(int i = 0; i < depth - 1; i++){
      blk_start = IN_DISK(BLK2ADDR(blk_start[INDIRECT_NUM_PER_BLK]));
      idx -= INDIRECT_NUM_PER_BLK;
    }
    Assert(idx >= 0 && idx < INDIRECT_NUM_PER_BLK, "invalid idx %d, expected [0, %ld)", idx, INDIRECT_NUM_PER_BLK);
    if(idx == (INDIRECT_NUM_PER_BLK - 1)){ // alloc a new page
      int newblk_idx = alloc_block();
      blk_start[INDIRECT_NUM_PER_BLK] = newblk_idx;
      blk_start = IN_DISK(BLK2ADDR(newblk_idx));
      blk_start[0] = blk_idx;
    }else{
      blk_start[idx] = blk_idx;
    }
  }
}
/* get the disk block idx for a given block idx in inode*/
static int get_blk_idx(int idx, inode_t* inode){
  if(idx < MAX_DIRECT_FILE_BLOCK) return inode->addr[idx];
  idx -= MAX_DIRECT_FILE_BLOCK;
  Assert(idx < (MAX_DIRECT_FILE_BLOCK + inode->addr[DEPTH_IN_INODE] * INDIRECT_NUM_PER_BLK), "idx %d is out of dir with depth %d", idx, inode->addr[DEPTH_IN_INODE]);
  uint32_t* blk_start = IN_DISK(BLK2ADDR(inode->addr[INDIRECT_IN_INODE]));
  int depth = UP_BLK_NUM(idx, INDIRECT_NUM_PER_BLK);
  for(int i = 0; i < depth - 1; i++){
    blk_start = IN_DISK(BLK2ADDR(blk_start[INDIRECT_NUM_PER_BLK]));
    idx -= INDIRECT_NUM_PER_BLK;
  }
  Assert(idx < INDIRECT_NUM_PER_BLK, "idx %d INDIRECT_NUM_PER_BLK %ld", idx, INDIRECT_NUM_PER_BLK);
  return blk_start[idx];
}

static void insert_into_dir(int parent_inode, int child_inode, char* name){
  inode_t* parent = (inode_t*)IN_DISK(INODE_ADDR(parent_inode));
  int entry_idx = (parent->size) / sizeof(diren_t);  // number of entries
  Assert(parent->size % sizeof(diren_t) == 0, "size 0x%x dirent 0x%lx", parent->size, sizeof(diren_t));
  void* dirent_blk_start;
  int insert_idx = entry_idx % DIREN_PER_BLOCK;   // entry offset in selected blk
  int name_len = strlen(name);
  diren_t* pre_dirent = NULL;
  int has_start = 0;
  while(name_len){
    if(insert_idx == 0){ // alloc a new block for parent
      int newblk_idx = alloc_block();
      insert_blk_into_inode(parent, newblk_idx);
      dirent_blk_start = IN_DISK(BLK2ADDR(newblk_idx));
      insert_idx = 0;
    }else{
      int diren_blk_idx = entry_idx / DIREN_PER_BLOCK;   // blk idx in inode
      diren_blk_idx = get_blk_idx(diren_blk_idx, parent);           // blk idx in disk
      dirent_blk_start = IN_DISK(BLK2ADDR(diren_blk_idx));
    }
    diren_t* child = (diren_t*)dirent_blk_start + insert_idx;
    strncpy(child->name, name, DIREN_NAME_LEN);
    name_len = MAX(0, name_len - DIREN_NAME_LEN);
    if(pre_dirent){
      pre_dirent->next_entry = entry_idx;
      pre_dirent->type = has_start ? DIRENT_MID : DIRENT_START;
      has_start = 1;
    }
    insert_idx ++; entry_idx ++;
    parent->size += sizeof(diren_t);
    pre_dirent = child;
  }
  Assert(pre_dirent, "parent %d child %d name %s", parent_inode, child_inode, name);
  pre_dirent->inode_idx = child_inode;
  pre_dirent->type = has_start ? DIRENT_END : DIRENT_SINGLE;
}

static int new_inode(int type){
  static int inode_idx = 0;
  inode_t* inode = (inode_t*)IN_DISK(INODE_ADDR(inode_idx));
  inode->type = type;
  inode->n_link = 1;
  inode->size = 0;
  memset(inode->addr, 0, sizeof(inode->addr));
  return inode_idx ++;
}

static void createFileList(int root, char *basePath){
  DIR *dir;
  struct dirent *ptr;
  char base[1000];
  Assert((dir=opendir(basePath)), "Open dir %s error...", basePath);

  while ((ptr=readdir(dir))){
    if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0) continue;
    else if(ptr->d_type == 8){    //file
      int newfile_inode_no = new_inode(FT_FILE);
      insert_into_dir(root, newfile_inode_no, ptr->d_name);
      memset(base, 0, sizeof(base));
      strcpy(base, basePath);
      strcat(base, "/");
      strcat(base, ptr->d_name);
      int fd = open(base, O_RDONLY);
      struct stat statbuf;
      stat(base, &statbuf);
      inode_t* newfile_inode = (inode_t*)IN_DISK(INODE_ADDR(newfile_inode_no));
      newfile_inode->size = statbuf.st_size;
      uint32_t blk_offset = 0;
      uint32_t left_size = statbuf.st_size;
      uint8_t* file_buf;
      while(left_size){
        uint32_t tmp_size = MIN(BLK_SIZE, left_size);
        file_buf = mmap(NULL, tmp_size, PROT_READ, MAP_SHARED, fd, blk_offset * BLK_SIZE);
        int newblk_no = alloc_block();
        memcpy(IN_DISK(BLK2ADDR(newblk_no)), file_buf, tmp_size);
        if(blk_offset < MAX_DIRECT_FILE_BLOCK) newfile_inode->addr[blk_offset] = newblk_no;
        else{
          int tmp_depth = (blk_offset - MAX_DIRECT_FILE_BLOCK) / INDIRECT_NUM_PER_BLK;
          int tmp_offset = (blk_offset - MAX_DIRECT_FILE_BLOCK) % INDIRECT_NUM_PER_BLK;
          uint32_t* tmp_blk_start = IN_DISK(BLK2ADDR(newfile_inode->addr[INDIRECT_IN_INODE]));
          while(tmp_depth --){
            if(!tmp_depth && !tmp_offset){
              *(uint32_t*)IN_DISK(BLK2ADDR(tmp_blk_start[INDIRECT_NUM_PER_BLK])) = alloc_block();
            }
            tmp_blk_start = IN_DISK(BLK2ADDR(tmp_blk_start[INDIRECT_NUM_PER_BLK]));
          }
          tmp_blk_start[tmp_offset] = newblk_no;
        }
      }
    }
    else if(ptr->d_type == 10){    //link file (not supported yet)
      Assert(0, "file %s/%s is a link file", basePath, ptr->d_name);
    }
    else if(ptr->d_type == 4){    ///dir
      int newdir_inode = new_inode(FT_DIR);
      insert_into_dir(root, newdir_inode, ptr->d_name);
      memset(base,'\0',sizeof(base));
      strcpy(base,basePath);
      strcat(base,"/");
      strcat(base,ptr->d_name);
      createFileList(newdir_inode, base);
    }
  }
  closedir(dir);
}

static diren_t* find_dirent_by_idx(int idx, inode_t* inode){
  int diren_blk_idx = idx / DIREN_PER_BLOCK;
  int idx_in_blk = idx % DIREN_PER_BLOCK;
  diren_t* blk_start = IN_DISK(BLK2ADDR(get_blk_idx(diren_blk_idx, inode)));
  return blk_start + idx_in_blk;
}

static void readFileList(int root, int depth){
  inode_t* root_inode = (inode_t*)IN_DISK(INODE_ADDR(root));
  if(root_inode->type != FT_DIR) return;
  int entry_num = root_inode->size / sizeof(diren_t);
  int blk_idx = 0;
  int entry_idx = 0;
  diren_t* blk_start = NULL;
  for(entry_idx = 0; entry_idx < entry_num; entry_idx++){
    if(entry_idx == DIREN_PER_BLOCK){
      entry_idx -= DIREN_PER_BLOCK; entry_num -= DIREN_PER_BLOCK;
    }
    if(entry_idx == 0) blk_start = IN_DISK(BLK2ADDR(get_blk_idx(blk_idx, root_inode)));
    diren_t* current = blk_start + entry_idx;
    if(current->type == DIRENT_SINGLE){
      for(int i = 0; i < depth; i++) printf("  ");
      printf("%s\n", current->name);
      if((strcmp(current->name, ".") == 0) || (strcmp(current->name, "..") == 0)) continue;
    }else if(current->type == DIRENT_START){
      for(int i = 0; i < depth; i++) printf("  ");
      while(current->type != DIRENT_END){
        printf("%s", current->name);
        current = find_dirent_by_idx(current->next_entry, root_inode);
      }
      printf("%s\n", current->name);
    }else{ continue; }
    readFileList(current->inode_idx, depth + 1);
  }
}

/* mkfs n(MB) kernel fs-img */
int main(int argc, char *argv[]) {
  int fd, size = atoi(argv[1]) << 20;

  Assert(argc == 4, "Usage: mkfs size fs.img dir");
  Assert(size > (1 MB), "size should be larger than 1 MB, get %s", argv[1]);

  assert((fd = open(argv[2], O_RDWR)) > 0);

  // struct stat statbuf;
  // fstat(fd,&statbuf);
  // Assert(statbuf.st_size <= (1 MB), "input file %s is larger than 1 MB", argv[2]);
  // TODO: check whether kernel.elf is larger than 1M
  // TODO: argument parsing

  assert((ftruncate(fd, size)) == 0);
  assert((disk = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) != (void *)-1);

  // TODO: mkfs

  memset(disk + FS_START, 0, size - FS_START);

  Assert(LOG_SIZE >= BLK_SIZE / 2, "BLK SIZE (0x%x) is too small", BLK_SIZE);
  /* superblock */
  uint32_t fssize = size - FS_START;
  superblock_t* sb = (superblock_t*)IN_DISK(SUPER_BLOCK_ADDR);
  uint32_t fs_blk_num = fssize / BLK_SIZE;
  uint32_t bitmap_blk_num = UP_BLK_NUM(fs_blk_num, 8 * BLK_SIZE);
  uint32_t meta_num = 1 + INODE_BLK_NUM + bitmap_blk_num;
  sb->super_magic = SUPER_MAGIC;
  sb->fssize = fssize;
  sb->n_blk = fs_blk_num - meta_num;
  Assert(fssize % BLK_SIZE == 0, "invalid fssize %x", fssize);
  sb->n_inode = INODE_BLK_NUM;
  sb->n_log = N_LOG;
  sb->inode_start = FS_BLK + 1;
  sb->bitmap_start = sb->inode_start + INODE_BLK_NUM;
  sb->data_start = sb->bitmap_start + bitmap_blk_num;
  sb->used_blk = 0;

  int root_idx = new_inode(FT_DIR);
  /* create . and ..*/
  insert_into_dir(root_idx, root_idx, ".");
  insert_into_dir(root_idx, root_idx, "..");
  /* create fs */
  createFileList(root_idx, argv[3]);

#ifdef MKFS_DEBUG
  readFileList(root_idx, 0);
#endif

  munmap(disk, size);
  close(fd);
}

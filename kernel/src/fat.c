#include <common.h>
#include <kmt.h>
#include <user.h>
#include <dev_sim.h>
#include <fat32.h>
#include <fs.h>
#include <fpioa.h>
#include <sysctl.h>
#include <dev_sim.h>
#ifdef FS_FAT32


int dev_input_read(ofile_t* ofile, int fd, void *buf, int count);
int dev_output_write(ofile_t* ofile, int fd, void *buf, int count);
int dev_error_write(ofile_t* ofile, int fd, void *buf, int count);


static int file_read(ofile_t* ofile, int fd, void *buf, int count);
static int file_lseek(ofile_t* ofile, int fd, int offset, int whence);
static int file_write(ofile_t* ofile, int fd, void *buf, int count);

static device_t* dev_sd;
#define sd_op dev_sd->ops
#define sd_read(offset, buf, count) sd_op->read(dev_sd, offset, buf, count)
#define sd_write(offset, buf, count) sd_op->write(dev_sd, offset, buf, count)

static FAT32_BS fat32_bs;
static dirent_t root;
static uint8_t fat_buf[512];

static sem_t fs_lock;

static ofile_t* stdin_info;
static ofile_t* stdout_info;
static ofile_t* stderr_info;

int invalid_write(ofile_t* ofile, int fd, void *buf, int count);
int invalid_read(ofile_t* ofile, int fd, void *buf, int count);
int invalid_lseek(ofile_t* ofile, int fd, int offset, int whence);

void init_stdfd(){
  stdin_info = pmm->alloc(sizeof(ofile_t));
	stdin_info->write = invalid_write;
	stdin_info->read = dev_input_read;
	stdin_info->lseek = invalid_lseek;
	stdin_info->count = 1;
	kmt->sem_init(&stdin_info->lock, "stdin_info lock", 1);

	stdout_info = pmm->alloc(sizeof(ofile_t));
	stdout_info->write = dev_output_write;
	stdout_info->read = invalid_read;
	stdout_info->lseek = invalid_lseek;
	stdout_info->count = 1;
	kmt->sem_init(&stdout_info->lock, "stdout_info lock", 1);

	stderr_info = pmm->alloc(sizeof(ofile_t));
	stderr_info->write = dev_error_write;
	stderr_info->read = invalid_read;
	stderr_info->lseek = invalid_lseek;
	stderr_info->count = 1;
	kmt->sem_init(&stderr_info->lock, "stderr_info lock", 1);
}

static void fat_init(){

  fpioa_pin_init();
  dmac_init();
  sdcard_init();

  dev_sd = dev->lookup("sda");
	sd_op->init(dev_sd);

	sd_read(0, fat_buf, 512);
  printf("fat32 %s\n", (char*)&fat_buf[82]);
  memmove(&fat32_bs.BPB_BytsPerSec, fat_buf + 11, 2);  // misaligned
  fat32_bs.BPB_SecPerClus = fat_buf[13];
  fat32_bs.BPB_RsvdSecCnt = *(uint16_t*)&fat_buf[14];
  fat32_bs.BPB_NumFATs = fat_buf[16];
  fat32_bs.BPB_HiddSec = *(uint32_t*)&fat_buf[28];
  fat32_bs.BPB_TotSec32 = *(uint32_t*)&fat_buf[32];
  fat32_bs.BPB_FATSz32 = *(uint32_t*)&fat_buf[36];
  fat32_bs.BPB_RootClus = *(uint32_t*)&fat_buf[44];
  printf("BPB_BytsPerSec 0x%x \nBPB_SecPerClus 0x%x \nBPB_RsvdSecCnt 0x%x \nBPB_NumFATs 0x%x \nBPB_HiddSec 0x%x \nBPB_TotSec32 0x%x \nBPB_FATSz32 0x%x \nBPB_RootClus 0x%x\n", fat32_bs.BPB_BytsPerSec , fat32_bs.BPB_SecPerClus, fat32_bs.BPB_RsvdSecCnt, fat32_bs.BPB_NumFATs, fat32_bs.BPB_HiddSec, fat32_bs.BPB_TotSec32, fat32_bs.BPB_FATSz32, fat32_bs.BPB_RootClus);
  fat32_bs.BytePerClus = fat32_bs.BPB_BytsPerSec * fat32_bs.BPB_SecPerClus;
  fat32_bs.RsvByte = fat32_bs.BPB_RsvdSecCnt * fat32_bs.BPB_BytsPerSec;
  fat32_bs.DataStartByte = fat32_bs.RsvByte + fat32_bs.BPB_NumFATs * fat32_bs.BPB_FATSz32 * fat32_bs.BPB_BytsPerSec;
  root.attr = ATTR_DIRECTORY | ATTR_SYSTEM;
  root.FstClus = fat32_bs.BPB_RootClus;
  root.parent = &root;
  strcpy(root.name, "/");
  init_stdfd();
  kmt->sem_init(&fs_lock, "fs lock", 1);
  return;
}

int invalid_write(ofile_t* ofile, int fd, void *buf, int count){
	return -1;
}

int invalid_read(ofile_t* ofile, int fd, void *buf, int count){
	return -1;
}

int invalid_lseek(ofile_t* ofile, int fd, int offset, int whence){
	return -1;
}

static inline int fat_sec_of_clus(uint32_t clus, uint32_t fatid){
  return fat32_bs.BPB_RsvdSecCnt + (clus * sizeof(uint32_t)) / fat32_bs.BPB_BytsPerSec  + (fatid-1) * fat32_bs.BPB_FATSz32;
}

static inline uint32_t fat_offfset_of_clus(uint32_t clus, uint32_t fatid){
  return fat32_bs.RsvByte + clus * sizeof(uint32_t) + fatid * fat32_bs.BPB_FATSz32 * fat32_bs.BPB_BytsPerSec;
}

static inline uint32_t get_clus_start(uint32_t clus){
  return fat32_bs.DataStartByte + (clus - 2) * fat32_bs.BytePerClus;
}

static int fat_write(int fd, void *buf, int count){
	task_t* cur_task = kmt->gettask();
	if(!IS_VALID_FD(fd) || !cur_task->ofiles[fd]){
		printf("write: invalid fd %d\n", fd);
		return -1;
	}
	return cur_task->ofiles[fd]->write(cur_task->ofiles[fd], fd, buf, count);
}

static int fat_read(int fd, void *buf, int count){
  task_t* cur_task = kmt->gettask();
	if(!IS_VALID_FD(fd) || !cur_task->ofiles[fd]){
		printf("read: invalid fd %d\n", fd);
		return -1;
	}
	return cur_task->ofiles[fd]->read(cur_task->ofiles[fd], fd, buf, count);
}

static int fat_close(int fd){
	task_t* cur_task = kmt->gettask();
	if(fd < 0 || fd >= MAX_OPEN_FILE || !cur_task->ofiles[fd]){
		printf("close: file %d is not open\n", fd);
		return -1;
	}
	if(--cur_task->ofiles[fd]->count == 0){
		pmm->free(cur_task->ofiles[fd]);
	}
	cur_task->ofiles[fd] = NULL;
	return 0;
}


dirent_t* dup_dirent(dirent_t * dirent){
  dirent->ref ++;
  return dirent;
}

static uint32_t next_clus(uint32_t clus){
  uint32_t ret;
  sd_read(fat_offfset_of_clus(clus, 0), &ret, sizeof(uint32_t));
  return ret;
}

static uint32_t alloc_clus(){
  uint32_t clus_count = fat32_bs.BPB_FATSz32 * fat32_bs.BPB_BytsPerSec / sizeof(uint32_t);
  for(int i = 0; i < clus_count; i++){
    uint32_t data;
    uint32_t offset = fat32_bs.RsvByte + i * sizeof(uint32_t);
    sd_read(offset, &data, sizeof(uint32_t));
    if(data == 0){
      data = FAT32_EOF;
      sd_write(offset, &data, sizeof(uint32_t));
      return i;
    }
  }
  Assert(0, "no avaliable cluster");
}

static void link_clus(uint32_t prev, uint32_t next){
  sd_write(fat_offfset_of_clus(prev, 0), &next, sizeof(uint32_t));
  sd_write(fat_offfset_of_clus(prev, 1), &next, sizeof(uint32_t));
}

static void free_clus(uint32_t clus){
  uint32_t data = 0;
  sd_write(fat32_bs.RsvByte + clus * sizeof(uint32_t), &data, sizeof(uint32_t));
}

static dirent_t* alloc_dirent(){
  dirent_t* dirent = pmm->alloc(sizeof(dirent_t));
  memset(dirent, 0, sizeof(dirent_t));
  dirent->ref = 1;
  return dirent;
}

static void free_dirent(dirent_t* dirent){
  Assert(dirent->ref > 0, "invalid dirent %s ref %d", dirent->name, dirent->ref);
  dirent->ref --;
  if(dirent->ref == 0){
    pmm->free(dirent);
  }
}

static int split_base_name(char* name){
	int name_idx;
	for(name_idx = strlen(name) - 1; name_idx >= 0; name_idx --){
		if(name[name_idx] == '/'){
			name[name_idx] = 0;
			break;
		}
	}
	return name_idx;
}

void wc2c(uint8_t* dst, uint8_t* src, int len){

  for(; len > 0 && (*src || *(src + 1)); len--){
    *dst ++ = *src;
    src += 2;
  }

  for(; len > 0; len--) *dst ++ = 0;

}

void c2wc(uint8_t* dst, uint8_t* src, int len){
  for(;len > 0 && *src; len--){  // avoid miaslign store
    *dst ++ = (*src ++);
    *dst ++ = 0;
  }
  if(len > 0){
    *dst ++ = 0;
    *dst ++ = 0;
    len --;
  }
  for(;len > 0; len--){
    *dst ++ = 0xff;
    *dst ++ = 0xff;
  }
}

void get_dirent_name(uint8_t* buf, fat32_dirent_t* fentry){
  if(fentry->ld.attr == ATTR_LONG_NAME){
    int sz = sizeof(fentry->ld.name1) / 2;
    wc2c(buf, fentry->ld.name1, sz);
    buf += sz;
    sz = sizeof(fentry->ld.name2) / 2;
    wc2c(buf, fentry->ld.name2, sz);
    buf += sz;
    sz = sizeof(fentry->ld.name3) / 2;
    wc2c(buf, fentry->ld.name3, sz);
  } else{
    for(int i = 0; i < 8 && fentry->sd.name[i] != ' '; i++){
      *buf++ = fentry->sd.name[i];
    }
    if(fentry->sd.name[8] != ' '){
      *buf ++ = '.';
    }
    for(int i = 8; i < 11 && fentry->sd.name[i] != ' '; i++){
      *buf ++ = fentry->sd.name[i];
    }
  }
  *buf ++ = 0;
}

static void get_dirent_info(dirent_t *dirent, fat32_dirent_t* fentry){
  dirent->attr = fentry->sd.attr;
  dirent->FstClus = ((uint32_t)fentry->sd.FstClusHi << 16) | fentry->sd.FstClusLo;
  dirent->FileSz = fentry->sd.FileSz;

}

static dirent_t* search_in_dir(dirent_t* dir, char* name){
  if(!(dir->attr & ATTR_DIRECTORY)) return NULL;
  if(strcmp(name, ".") == 0){
    return dup_dirent(dir);
  }else if(strcmp(name, "..") == 0){
    return dup_dirent(dir->parent);
  }

  uint32_t clus = dir->FstClus, clusOffset = 0;
  fat32_dirent_t fentry;

  dirent_t* dirent = alloc_dirent();

  int pre_is_ld = 0;
  int entry_num = 0;

  while(clus < FAT32_EOF){
    sd_read(get_clus_start(clus) + clusOffset, &fentry, 32);  // TODO: check offset
    if(fentry.ld.Ord == ENTRY_EMPTY){

    } else if(fentry.ld.attr == ATTR_LONG_NAME){
      int ord = fentry.ld.Ord & ~LAST_LONG_ENTRY;

      if(fentry.ld.Ord & LAST_LONG_ENTRY){
        pre_is_ld = 1;
        entry_num = fentry.ld.Ord & ~LAST_LONG_ENTRY;
      }
      get_dirent_name(dirent->name + (ord-1) * LONG_NAME_LENGTH, &fentry);

    } else{
      if(!pre_is_ld){
        get_dirent_name(dirent->name, &fentry);
      }
      get_dirent_info(dirent, &fentry);
      dirent->parent = dir;
      printf("name: (%s)\n", dirent->name);
      if(strcmp(dirent->name, name) == 0) return dirent;
      pre_is_ld = 0;
    }
    dirent->offset += 32;
    dirent->clus_in_parent = clus;
    clusOffset += 32;
    if(clusOffset >= fat32_bs.BytePerClus){
      clusOffset -= fat32_bs.BytePerClus;
      clus = next_clus(clus);
    }
  }
  free_dirent(dirent);
  return NULL;
}

static uint32_t compute_entry_num(char* name){
  return (strlen(name) + LONG_NAME_LENGTH - 1) / LONG_NAME_LENGTH + 1;
}

static uint32_t addr_for_dirent_offset(dirent_t* dirent, uint32_t offset){
  int clus_depth = (offset / fat32_bs.BytePerClus);
  int clus_offset = (offset % fat32_bs.BytePerClus);
  int clus = dirent->FstClus;
  while(clus_depth --){
    clus = next_clus(clus);
  }
  return get_clus_start(clus) + clus_offset;
}

static uint8_t illegal_schar[] = { 0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C};

int check_illegal_schar(uint8_t c){
  for(int i = 0; i < sizeof(illegal_schar); i++){
    if(c == illegal_schar[i]) return 1;
  }
  return 0;
}

uint8_t to_schar(uint8_t c){
  if(c >= 'a' && c <= 'z'){
      return c + 'A' - 'a';
    } else if(check_illegal_schar(c)){
      return '_';
    }
    return c;
}

void get_sname(char* sname, char* name){
  int point_idx = -1;
  for(int i = 0; i < strlen(name); i++){
    if(name[i] == '.') point_idx = i;
  }
  int idx = 0;
  for(idx = 0; idx < 8 && (point_idx < 0 || idx < point_idx); idx++){
    *sname ++ = to_schar(name[idx]);
  }
  while(idx ++ < 8) *sname ++ = ' ';
  while(idx ++ < SHORT_NAME_LANGTH && point_idx > 0 && point_idx < strlen(name)){
    *sname ++ = to_schar(name[point_idx ++]);
  }
  while(idx ++ < SHORT_NAME_LANGTH) *sname ++ = ' ';

}

uint8_t checksum(char* sname){
  uint8_t sum = 0;
  for (int i = SHORT_NAME_LANGTH; i != 0; i--) {
      sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sname++;
  }
  return sum;
}

static void insert_dirent_to_dirent(dirent_t *parent, dirent_t* child, uint32_t offset, int entryNum){
  fat32_dirent_t fentry;
  memset(&fentry, 0, sizeof(fat32_dirent_t));
  if(entryNum == 1){  // root does not contain "." and ".."
    Assert(parent != &root && offset <= 32, "insert (%s) -> (%s) offset %d entryNum %d", child->name, parent->name, offset, entryNum);
    if(offset == 0){
      strcpy(fentry.sd.name, ".          ");  // trailing space padded
    } else if(offset == 32){
      strcpy(fentry.sd.name, "..         ");
    } else{
      Assert(0, "invalid offset %d\n", offset);
    }
    fentry.sd.attr = ATTR_DIRECTORY;
    fentry.sd.FstClusHi = child->FstClus >> 16;
    fentry.sd.FstClusLo = child->FstClus & (((uint32_t)1 << 16) - 1);
    fentry.sd.FileSz = child->FileSz;
    sd_write(addr_for_dirent_offset(parent, offset), &fentry, 32);
  } else{
    char sname[SHORT_NAME_LANGTH + 1];
    memset(sname, 0, sizeof(sname));
    get_sname(sname, child->name);
    fentry.ld.Chksum = checksum(sname);
    fentry.ld.attr = ATTR_LONG_NAME;
    for(int i = entryNum; i > 0; i--){  // long name
      fentry.ld.Ord = i | (i == entryNum ? LAST_LONG_ENTRY : 0);
      uint8_t* str = child->name + (i - 1) * LONG_NAME_LENGTH;
      uint8_t wc[LONG_NAME_LENGTH];
      c2wc(wc, str, LONG_NAME_LENGTH);
      memmove(fentry.ld.name1, wc, 10);
      memmove(fentry.ld.name2, wc + 10, 12);
      memmove(fentry.ld.name3, wc + 22, 4);
      sd_write(addr_for_dirent_offset(parent, offset + (entryNum - i) * 32), &fentry, 32);
    }
    memset(&fentry, 0, sizeof(fentry));
    memmove(&fentry.sd.name, sname, SHORT_NAME_LANGTH);
    fentry.sd.attr = child->attr;
    fentry.sd.FstClusHi = child->FstClus >> 16;
    fentry.sd.FstClusLo = child->FstClus & (((uint32_t)1 << 16) - 1);
    fentry.sd.FileSz = child->FileSz;
    sd_write(addr_for_dirent_offset(parent, offset + entryNum * 32), &fentry, 32);
  }
}

static uint32_t search_empty_dirent(dirent_t *dirent, uint32_t num){
  if(!(dirent->attr & ATTR_DIRECTORY)) return NULL;
  

  uint32_t sec = 0, clus = dirent->FstClus, clusOffset = 0;
  fat32_dirent_t fentry;

  int empty_num = 0;
  uint32_t ret = 0;
  while(clus < FAT32_EOF){
    sd_read(get_clus_start(clus) + clusOffset, &fentry, 32);  // TODO: check offset
    if(fentry.ld.Ord == ENTRY_EMPTY){
      empty_num ++;
      if(empty_num >= num) return ret;
    } else{  // point to next entry
      ret += empty_num + 1;
    }
    clusOffset += 32;
    if(clusOffset >= fat32_bs.BytePerClus){
      clusOffset -= fat32_bs.BytePerClus;
      clus = next_clus(clus);
    }
  }
  Assert(0, "no empty %d dirent", num);
}


static dirent_t* create_in_dir(dirent_t* baseDir, char* name, int attr){
  dirent_t* dirent = alloc_dirent();
  dirent->FstClus = alloc_clus();
  dirent->parent = baseDir;
  dirent->FileSz = 0;
  dirent->attr = attr;
  strcpy(dirent->name, name);
  if(attr & ATTR_DIRECTORY){
    insert_dirent_to_dirent(dirent, dirent, 0, 1);
    insert_dirent_to_dirent(dirent, baseDir, 32, 1);
  } else{
    dirent->attr |= ATTR_ARCHIVE;
  }
  uint32_t entry_num = compute_entry_num(name);
  uint32_t select_entry_idx = search_empty_dirent(baseDir, entry_num);
  insert_dirent_to_dirent(baseDir, dirent, select_entry_idx * 32, entry_num);
  return dirent;
}


static dirent_t* fat_search(dirent_t* baseDir, char* path){
  char* token = path;
  int delim_idx = find_replace(path, "/", 0);
  while(token) {
    if(!baseDir || !(baseDir->attr & ATTR_DIRECTORY)) return NULL;
    baseDir = search_in_dir(baseDir, token);

    if(delim_idx == -1) break;
    path += delim_idx + 1;
    token = path;
    delim_idx = find_replace(path, "/", 0);
  }
  return baseDir;
}

static dirent_t* fat_create(dirent_t* baseDir, char* path, int flags){
  int name_idx = split_base_name(path);
  baseDir = fat_search(baseDir, path);
  if(!baseDir) return NULL;
  char* filename =  path + name_idx + 1;
  dirent_t* dirent = search_in_dir(baseDir,filename);
  if(dirent) return dirent;
  return create_in_dir(baseDir, filename, flags);
}

static int fat_open(const char *pathname, int flags){
  Assert(strlen(pathname) < FAT32_MAX_PATH_LENGTH, "pathname %s is too long\n", pathname);
  kmt->sem_wait(&fs_lock);
  dirent_t* baseDir = pathname[0] == '/' ? &root : kmt->gettask()->cwd;
  if(pathname[0] == '/') pathname ++;
	char string_buf[FAT32_MAX_PATH_LENGTH];
	strcpy(string_buf, pathname);

/* read file from disk */
  dirent_t* file;
  if(flags & O_CREAT){
    file = fat_create(baseDir, string_buf, 0);
  } else{
    file = fat_search(baseDir, string_buf);
  }
  if(!file){
    kmt->sem_signal(&fs_lock);
    printf("open: no such file or directory %s\n", pathname);
    return -1;
  }

	int writable = (flags & O_WRONLY) || (flags & O_RDWR);
	int readable = !(flags & O_WRONLY);


	ofile_t* tmp_ofile = pmm->alloc(sizeof(ofile_t));
	tmp_ofile->write = writable ? file_write : invalid_write;
	tmp_ofile->read = readable ? file_read : invalid_read;
	tmp_ofile->lseek = file_lseek;
	tmp_ofile->offset = (flags & O_APPEND) ? file->FileSz : 0;
	tmp_ofile->count = 1;
	tmp_ofile->dirent = file;
	tmp_ofile->type = 0;
	tmp_ofile->flag = flags;
	kmt->sem_init(&tmp_ofile->lock, pathname, 1);

	kmt->sem_signal(&fs_lock);
	return fill_task_ofile(tmp_ofile);
}

static int file_read(ofile_t* ofile, int fd, void *buf, int count){
  kmt->sem_wait(&fs_lock);
  uint32_t clus_depth = ofile->offset / fat32_bs.BytePerClus;
  uint32_t clus_offset = ofile->offset % fat32_bs.BytePerClus;
  dirent_t* file = ofile->dirent;
  uint32_t clus = file->FstClus;
  int read_size = MIN(file->FileSz - ofile->offset, count);
  int ret = read_size;
  while(clus_depth-- && clus < FAT32_EOF) clus = next_clus(clus);

  if(clus >= FAT32_EOF || read_size <= 0) {
    kmt->sem_signal(&fs_lock);
    return 0;
  }

  while(read_size){
    uint32_t clus_read_count = MIN(read_size, fat32_bs.BytePerClus - clus_offset);
    sd_read(get_clus_start(clus) + clus_offset, buf, clus_read_count);
    char* tmp = buf;
    read_size -= clus_read_count;
    buf += clus_read_count;
    if(read_size){
      clus = next_clus(clus);
      clus_offset = 0;
    }
  }
  ofile->offset += read_size;
  kmt->sem_signal(&fs_lock);
  return ret;
}

static int file_lseek(ofile_t* ofile, int fd, int offset, int whence){
  switch(whence){
		case SEEK_SET: ofile->offset = offset; break;
		case SEEK_CUR: ofile->offset += offset; break;
		case SEEK_END: ofile->offset = ofile->dirent->FileSz; break;
		default: Assert(0, "invalid whence %d", whence);
	}
	return ofile->offset;
}


static int file_write(ofile_t* ofile, int fd, void *buf, int count){
  kmt->sem_wait(&fs_lock);
  if(ofile->offset >= ofile->dirent->FileSz){
    TODO();
  }
  int32_t clus_depth = ofile->offset / fat32_bs.BytePerClus;
  uint32_t clus_offset = ofile->offset % fat32_bs.BytePerClus;
  dirent_t* file = ofile->dirent;
  uint32_t clus = file->FstClus;
  uint32_t prev_clus = 0;
  for(; clus_depth > 0 && clus < FAT32_EOF; clus_depth--) {
    prev_clus = clus;
    clus = next_clus(clus);
  }
  for(; clus_depth > 0; clus_depth--) {
    Assert(prev_clus != 0, "prev_clus == 0");
    clus = alloc_clus();
    link_clus(prev_clus, clus);
  }
  uint32_t write_count = count;
  while(write_count){
    uint32_t clus_write_count = MIN(write_count, fat32_bs.BytePerClus - clus_offset);
    sd_write(get_clus_start(clus) + clus_offset, buf, clus_write_count);
    write_count -= clus_write_count;
    buf += clus_write_count;
    if(write_count){
      prev_clus = clus;
      clus = next_clus(clus);
      if(clus >= FAT32_EOF){
        clus = alloc_clus();
        link_clus(prev_clus, clus);
      }
      clus_offset = 0;
    }
  }
  ofile->offset += count;
  ofile->dirent->FileSz = MAX(ofile->dirent->FileSz, ofile->offset);
  //update parent
  dirent_t* parent = ofile->dirent->parent;
  uint32_t dirent_addr = get_clus_start(ofile->dirent->clus_in_parent) + ofile->dirent->offset % fat32_bs.BytePerClus;

  sd_write(dirent_addr + STRUCT_OFFSET(fat32_dirent_t, sd.FileSz), &(ofile->dirent->FileSz), 32);
  
  kmt->sem_signal(&fs_lock);
  return count;
}

static int fat_lseek(int fd, int offset, int whence){
  task_t* cur_task = kmt->gettask();
	if(!IS_VALID_FD(fd) || !cur_task->ofiles[fd]){
		printf("lseek: invalid fd %d\n", fd);
		return -1;
	}
	return cur_task->ofiles[fd]->lseek(cur_task->ofiles[fd], fd, offset, whence);
}

static int fat_link(const char *oldpath, const char *newpath){
  TODO();
	return 0;
}

static int fat_unlink(const char *pathname){
  TODO();
	return 0;
}

static int fat_fstat(int fd, struct ufs_stat *buf){
  TODO();
	return 0;
}

static int fat_mkdir(const char *pathname){
	TODO();
	return 0;
}

static int fat_chdir(const char *path){
	TODO();
	return 0;
}

static int fat_dup(int fd){
	TODO();
	return -1;
}

ofile_t* filedup(ofile_t* ofile){
	TODO();
	return NULL;
}

void fileclose(ofile_t* ofile){
	TODO();
}

void fill_standard_fd(task_t* task){
	task->ofiles[0] = NULL;
	task->ofiles[1] = NULL;
	task->ofiles[2] = NULL;
}


MODULE_DEF(vfs) = {
	.init   = fat_init,
	.write  = fat_write,
	.read   = fat_read,
	.close  = fat_close,
	.open   = fat_open,
	.lseek  = fat_lseek,
	.link   = fat_link,
	.unlink = fat_unlink,
	.fstat  = fat_fstat,
	.mkdir  = fat_mkdir,
	.chdir  = fat_chdir,
	.dup    = fat_dup,
};


void init_task_cwd(task_t* task){
  task->cwd = &root;
}

void vfs_disp(void* args){
  search_in_dir(&root, "abcd");
  while(1) ;
}

#endif
#include <common.h>
#include <kmt.h>
#include <vfs.h>
#include <user.h>
#include <dev_sim.h>
static superblock_t* sb;
static fslog_t* log;
static device_t* dev_sd;
#define sd_op dev_sd->ops
#define sd_read(offset, buf, count) sd_op->read(dev_sd, offset, buf, count)
#define sd_write(offset, buf, count) sd_op->write(dev_sd, offset, buf, count)

#define MAX_STRING_BUF_LEN 128

static ofile_info_t* stdin_info;
static ofile_info_t* stdout_info;
static ofile_info_t* stderr_info;

static proc_inode_t* proc_dir = NULL;
static dev_inode_t* dev_start = NULL;
static int dev_num = 0;

static sem_t fs_lock;
static sem_t procfs_lock;

static void insert_into_proc_dir(proc_inode_t* parent_inode, proc_inode_t* child_inode, const char* name){
	Assert(parent_inode && (parent_inode->type == FT_PROC_DIR), "insert_into_proc_dir parent(0x%x) type %d", parent_inode, parent_inode->type);
	if(!parent_inode->mem) parent_inode->mem = pmm->alloc(4 * BLK_SIZE);
	Assert(BLK_SIZE - parent_inode->size >= sizeof(proc_diren_t), "proc size %x\n", parent_inode->size);
	Assert(parent_inode->size % sizeof(proc_diren_t) == 0, "invalid parent size %d", parent_inode->size);
	int idx = parent_inode->size / sizeof(proc_diren_t);
	proc_diren_t* insert_dirent = (proc_diren_t*)parent_inode->mem + idx;
	insert_dirent->inode = child_inode;
	Assert(strlen(name) < PROC_NAME_LEN, "name %s is too long", name);
	strcpy(insert_dirent->name, name);
	parent_inode->size += sizeof(proc_diren_t);
}

static proc_inode_t* insert_proc_inode(proc_inode_t* parent, char* name, const char* msg, int type){
	proc_inode_t* new_inode = pmm->alloc(sizeof(proc_inode_t));
	insert_into_proc_dir(parent, new_inode, name);
	new_inode->type = type;
	new_inode->size = strlen(msg);
	if(msg) new_inode->mem = pmm->alloc(BLK_SIZE);
	memcpy(new_inode->mem, msg, new_inode->size);
	return new_inode;
}

void vfs_proc_init(){
	if(proc_dir) return;

	proc_dir = pmm->alloc(sizeof(proc_inode_t));
	proc_dir->type = FT_PROC_DIR;
	proc_dir->size = 0;
	proc_dir->mem = NULL;
	/* add cpuinfo */
	insert_proc_inode(proc_dir, "cpuinfo", "name: xxxxx", FT_PROC_FILE);
	/* add meminfo */
	insert_proc_inode(proc_dir, "meminfo", "MemTotal: xxxxx", FT_PROC_FILE);
	/* add dispinfo */
	insert_proc_inode(proc_dir, "dispinfo", "WIDTH: 400\nHEIGHT: 300", FT_PROC_FILE);
	kmt->sem_init(&procfs_lock, "procfs lock", 1);
}

void vfs_dev_init(){
	if(dev_start) return;

	dev_start = pmm->alloc(sizeof(dev_inode_t) * MAX_DEV_NUM);
	dev_num = 0;
	/* add dev null */
	dev_inode_t* new_dev = dev_start + dev_num ++;
	new_dev->size = 0;
	strcpy(new_dev->name, "null");
	new_dev->read = null_read;
	new_dev->write = null_write;
	new_dev->lseek = null_lseek;
	/* add dev zero */
	new_dev = dev_start + dev_num ++;
	new_dev->size = 0;
	strcpy(new_dev->name, "zero");
	new_dev->read = zero_read;
	new_dev->write = invalid_write;
	new_dev->lseek = zero_lseek;
	/* add dev random */
	new_dev = dev_start + dev_num ++;
	new_dev->size = 0;
	strcpy(new_dev->name, "random");
	new_dev->read = random_read;
	new_dev->write = invalid_write;
	new_dev->lseek = random_lseek;
	/* add dev events */
	new_dev = dev_start + dev_num ++;
	new_dev->size = 0;
	strcpy(new_dev->name, "events");
	new_dev->read = events_read;
	new_dev->write = invalid_write;
	new_dev->lseek = useless_lseek;
}

static int proc_read(ofile_info_t* ofile, int fd, void* buf, int count){
	proc_inode_t* proc_inode = ofile->proc_inode;
	kmt->sem_wait(&procfs_lock);
	int ret = MIN(proc_inode->size - ofile->offset, count);
	if(ret <= 0) {
		kmt->sem_signal(&procfs_lock);
		return 0;
	}
	memcpy(buf, proc_inode->mem, ret);
	kmt->sem_signal(&procfs_lock);
	return ret;
}

static int proc_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
	proc_inode_t* proc_inode = ofile->proc_inode;
	switch(whence){
		case SEEK_SET: ofile->offset = offset; break;
		case SEEK_CUR: ofile->offset += offset; break;
		case SEEK_END:
			kmt->sem_wait(&procfs_lock);
			ofile->offset = proc_inode->size;
			kmt->sem_signal(&procfs_lock); break;
		default: Assert(0, "invalid whence %d", whence);
	}
	return ofile->offset;
}

void new_proc_init(int id, const char* name){
	char string_buf[32];
	memset(string_buf, 0, sizeof(string_buf));
	sprintf(string_buf, "%d", id);
	kmt->sem_wait(&procfs_lock);
	proc_inode_t* id_inode = insert_proc_inode(proc_dir, string_buf, NULL, FT_PROC_DIR);
	insert_proc_inode(id_inode, "name", name, FT_PROC_FILE);
	kmt->sem_signal(&procfs_lock);
}

void release_proc(proc_inode_t* inode){
	if(inode->type == FT_PROC_DIR){
		int num = inode->size / sizeof(proc_diren_t);
		for(int i = 0; i < num; i++){
			proc_diren_t* proc_dirent = (proc_diren_t*)inode->mem + i;
			release_proc(proc_dirent->inode);
		}
	}
	if(inode->mem){
		pmm->free(inode->mem);
	}
	pmm->free(inode);
}

int search_proc_dirent_idx(proc_inode_t* inode, char* name){
	proc_diren_t* start = inode->mem;
	Assert(start, "get_proc_idx: proc empty");
	int total_num = inode->size / sizeof(proc_inode_t);
	for(int i = 0; i < total_num; i++){
		if(strcmp(start[i].name, name) == 0){
			return i;
		}
	}
	Assert(0, "invalid name %s in inode 0x%lx", name, inode);
}

int get_proc_idx(int id){
	char string_buf[32];
	memset(string_buf, 0, sizeof(string_buf));
	sprintf(string_buf, "%d", id);
	return search_proc_dirent_idx(proc_dir, string_buf);
}

void delete_proc(int pid){
	kmt->sem_wait(&procfs_lock);
	int idx = get_proc_idx(pid);
	proc_diren_t* select = (proc_diren_t*)proc_dir->mem + idx;
	release_proc(select->inode);
	int total_num = proc_dir->size / sizeof(proc_diren_t);
	if(idx != total_num - 1){
		proc_diren_t* last = (proc_diren_t*)proc_dir->mem + total_num - 1;
		memcpy(select, last, sizeof(proc_diren_t));
	}
	proc_dir->size -= sizeof(proc_diren_t);
	kmt->sem_signal(&procfs_lock);
}

void modify_proc_info(int pid, char* file_name, void* data, int sz){
	kmt->sem_wait(&procfs_lock);
	int idx = get_proc_idx(pid);
	proc_inode_t* proc_inode = ((proc_diren_t*)proc_dir->mem + idx)->inode;
	int file_idx = search_proc_dirent_idx(proc_inode, file_name);
	proc_inode_t* file_inode = ((proc_diren_t*)proc_inode->mem + file_idx)->inode;
	memcpy(file_inode->mem, data, sz);
	file_inode->size = sz;
	kmt->sem_signal(&procfs_lock);
}

int invalid_write(ofile_info_t* ofile, int fd, void *buf, int count){
	return -1;
}

int invalid_read(ofile_info_t* ofile, int fd, void *buf, int count){
	return -1;
}

int invalid_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
	return -1;
}

static void get_inode_by_no(int inode_no, inode_t* inode){
	sd_read(INODE_ADDR(inode_no), inode, sizeof(inode_t));
}

/* get the disk block idx for a given block idx in inode*/
static int get_blk_idx(int idx, inode_t* inode){
  if(idx < MAX_DIRECT_FILE_BLOCK) return inode->addr[idx];
  idx -= MAX_DIRECT_FILE_BLOCK;
  Assert(idx < (MAX_DIRECT_FILE_BLOCK + inode->addr[DEPTH_IN_INODE] * INDIRECT_NUM_PER_BLK), "idx %d is out of dir with depth %d", idx, inode->addr[DEPTH_IN_INODE]);
  int blk_start = BLK2ADDR(inode->addr[INDIRECT_IN_INODE]);
  int depth = inode->addr[DEPTH_IN_INODE];
  for(int i = 0; i < depth - 1; i++){
		uint32_t next_blk;
		sd_read(blk_start + INDIRECT_NUM_PER_BLK * sizeof(uint32_t), &next_blk, sizeof(uint32_t));
		blk_start = BLK2ADDR(next_blk);
		idx -= INDIRECT_NUM_PER_BLK;
  }
  Assert(idx < INDIRECT_NUM_PER_BLK, "idx %d INDIRECT_NUM_PER_BLK %ld", idx, INDIRECT_NUM_PER_BLK);
	int ret;
	sd_read(blk_start + idx * sizeof(uint32_t), &ret, sizeof(uint32_t));
  return ret;
}

static int alloc_blk(){
	int bitmap_blk_start = BLK2ADDR(sb->bitmap_start);
	int search_idx = 0;
	uint32_t bitmap;
	for(search_idx = 0; search_idx < sb->n_blk; search_idx += 32){
		sd_read(bitmap_blk_start + (search_idx / 32 * 4), &bitmap, sizeof(uint32_t));
		if(bitmap == MAX32bit) continue;
		int free_bit = __builtin_ctz(~bitmap);
		bitmap = bitmap | ((uint32_t)1 << free_bit);
		sd_write(bitmap_blk_start + (search_idx / 32 * 4), &bitmap, sizeof(uint32_t));
		return sb->data_start + search_idx + free_bit;
	}
	printf("alloc: no avaliable block\n");
	return -1;
}

static int free_blk(int blk_no){
	blk_no -= sb->data_start;
	int bitmap_blk_start = BLK2ADDR(sb->bitmap_start);
	uint32_t bitmap;
	sd_read(bitmap_blk_start + (blk_no / 32 * 4), &bitmap, sizeof(uint32_t));
	Assert(bitmap & (uint32_t)1 << (blk_no & 0x1f), "blk %d is not allocated", blk_no);
	bitmap = bitmap & (~((uint32_t)1 << (blk_no & 0x1f)));
	sd_write(bitmap_blk_start + blk_no / 32 * 4, &bitmap, sizeof(uint32_t));
	return 0;
}

static int alloc_inode(int type, inode_t* inode){
	int inode_blk_start = BLK2ADDR(sb->inode_start);
	int inode_start = inode_blk_start;
	for(int inode_no = 0; inode_no < N_INODE; inode_no ++){
		sd_read(inode_start, inode, sizeof(inode_t));
		if(inode->type == FT_UNUSED){
			inode->type = type;
			inode->n_link = 1;
			inode->size = 0;
			memset(inode->addr, 0, sizeof(inode->addr));
			sd_write(inode_start, inode, sizeof(inode_t));
			return inode_no;
		}
		inode_start += sizeof(inode_t);
	}
	printf("no available inode\n");
	return -1;
}

static int free_inode(int no){
	int inode_start = BLK2ADDR(sb->inode_start) + no * sizeof(inode_t);
	inode_t inode = {.type = FT_UNUSED};
	sd_write(inode_start + OFFSET_IN_STRUCT(inode, type), &inode.type, sizeof(int));
	return 0;
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

static int link_inodeno_by_inode(inode_t* inode){
	Assert(inode->type == FT_LINK, "inode is not a link %d", inode->type);
	return inode->link_no;
}

static int __attribute__ ((unused)) link_inodeno_by_no(int inode_no){
	inode_t inode;
	get_inode_by_no(inode_no, &inode);
	return link_inodeno_by_inode(&inode);
}

static int insert_blk_into_inode(int inode_no, inode_t* inode, int insert_idx){
	int blk_num = UP_BLK_NUM(inode->size, BLK_SIZE);
  if(blk_num < MAX_DIRECT_FILE_BLOCK){
    inode->addr[blk_num] = insert_idx;
		sd_write(INODE_ADDR(inode_no), inode, sizeof(inode_t));
  }else{
    int idx = blk_num - MAX_DIRECT_FILE_BLOCK;
		if(idx == 0){
			inode->addr[INDIRECT_IN_INODE] = alloc_blk();
			inode->addr[DEPTH_IN_INODE] = 1;
			sd_write(INODE_ADDR(inode_no) + OFFSET_IN_PSTRUCT(inode, addr[DEPTH_IN_INODE]), &inode->addr[DEPTH_IN_INODE], 2 * sizeof(int));
		}
		int blk_start = BLK2ADDR(inode->addr[INDIRECT_IN_INODE]);
		int depth = UP_BLK_NUM(idx, INDIRECT_NUM_PER_BLK);
		for(int i = 0; i < depth - 1; i++){
			int next_blk;
			sd_read(blk_start + INDIRECT_NUM_PER_BLK * sizeof(int), &next_blk, sizeof(int));
      blk_start = BLK2ADDR(next_blk);
      idx -= INDIRECT_NUM_PER_BLK;
    }
    Assert(idx >= 0 && idx < INDIRECT_NUM_PER_BLK, "invalid idx %d, expected [0, %ld)", idx, INDIRECT_NUM_PER_BLK);
    if(idx == (INDIRECT_NUM_PER_BLK - 1)){ // alloc a new page
			inode->addr[DEPTH_IN_INODE] ++ ;
			sd_write(INODE_ADDR(inode_no) + OFFSET_IN_PSTRUCT(inode, addr[DEPTH_IN_INODE]), &inode->addr[DEPTH_IN_INODE], sizeof(int));
      int newblk_idx = alloc_blk();
			sd_write(blk_start + INDIRECT_NUM_PER_BLK * sizeof(int), &newblk_idx, sizeof(int));
      blk_start = BLK2ADDR(newblk_idx);
			sd_write(blk_start, &insert_idx, sizeof(int));
    }else{
			sd_write(blk_start + idx * sizeof(int), &insert_idx, sizeof(int));
    }
	}
	return 0;
}

static void get_dirent_by_idx(inode_t* inode, int idx, diren_t* diren){
	int blk_idx = idx * sizeof(diren_t) / BLK_SIZE;
	int blk_offset = (idx * sizeof(diren_t)) % BLK_SIZE;
	int left_size = sizeof(diren_t);
	while(left_size){
		int read_size = MIN(left_size, BLK_SIZE - blk_offset);
		sd_read(BLK2ADDR(get_blk_idx(blk_idx, inode)) + blk_offset, (void*)diren + sizeof(diren_t) - left_size, read_size);
		left_size -= read_size;
		blk_offset = 0;
		blk_idx += 1;
	}
}

static int search_inodeno_in_dir(inode_t* dir_inode, char* name){
	int entry_num = dir_inode->size / sizeof(diren_t);
	diren_t direntry;
	for(int i = 0; i < entry_num; i++){
		get_dirent_by_idx(dir_inode, i, &direntry);
		if(!strcmp(direntry.name, name)) return direntry.inode_idx;
	}
	return -1;
}

static void remove_inode_blk_by_inode(inode_t* inode){ // free all blks in inode
	int blk_num = UP_BLK_NUM(inode->size, BLK_SIZE);
	int free_idx = 0;
	for(free_idx = 0; free_idx < MIN(blk_num, MAX_DIRECT_FILE_BLOCK); free_idx++){
		free_blk(inode->addr[free_idx]);
	}
	if(free_idx >= blk_num) return;
	int left_blk_num = blk_num - free_idx;
	int depth = inode->addr[DEPTH_IN_INODE];
	int indirect_blk_idx = inode->addr[INDIRECT_IN_INODE];
	uint8_t blk_buf[BLK_SIZE];
	for(int i = 0; i < depth; i++){
		sd_read(BLK2ADDR(indirect_blk_idx), blk_buf, BLK_SIZE);
		int blk_left = MIN(left_blk_num, INDIRECT_NUM_PER_BLK);
		while(--blk_left >= 0) free_blk(*(uint32_t*)(blk_buf + blk_left * sizeof(int)));
		indirect_blk_idx = *(uint32_t*)(blk_buf + BLK_SIZE - sizeof(int));
	}
}

static void insert_dirent(diren_t* diren, inode_t* inode, int inode_no){
	int inode_blk_idx = (inode->size) / BLK_SIZE;
  int offset = inode->size % BLK_SIZE;
  int dirent_blk_start;
  int left_size = sizeof(diren_t);
  void* insert_pos = (void*)diren;
  while(left_size){
    int insert_size = MIN(BLK_SIZE - offset, left_size);
    if(offset == 0){ // alloc a new block
      int newblk_idx = alloc_blk();
      insert_blk_into_inode(inode_no, inode, newblk_idx);
      dirent_blk_start = BLK2ADDR(newblk_idx);
    }else{
      dirent_blk_start = BLK2ADDR(get_blk_idx(inode_blk_idx, inode));
    }
		sd_write(dirent_blk_start + offset, insert_pos, insert_size);
    insert_pos += insert_size;
    left_size -= insert_size;
    offset = 0;
		inode->size += insert_size;
  }
	sd_write(INODE_ADDR(inode_no), inode, sizeof(inode_t));
}

static void insert_into_dir(int parent_inode, int child_inode, char* name){
	inode_t parent;
	sd_read(INODE_ADDR(parent_inode), &parent, sizeof(inode_t));
  Assert(parent.size % sizeof(diren_t) == 0, "size 0x%x dirent 0x%lx", parent.size, sizeof(diren_t));
	Assert(parent.type == FT_DIR, "parent is not a dir, type %d\n", parent.type);
	diren_t pre_dirent;
	pre_dirent.inode_idx = child_inode;
  pre_dirent.type = DIRENT_SINGLE;
	strncpy(pre_dirent.name, name, DIREN_NAME_LEN);
	insert_dirent(&pre_dirent, &parent, parent_inode);
}

void fill_standard_fd(task_t* task){
	task->ofiles[0] = stdin_info;
	task->ofiles[1] = stdout_info;
	task->ofiles[2] = stderr_info;
}

static void vfs_init(){
	// TODO: recover from log
	dev_sd = dev->lookup("sda");
	sd_op->init(dev_sd);
	sb = pmm->alloc(BLK_SIZE);
	log = (fslog_t*)(sb + 1);
	sd_read(FS_START, sb, BLK_SIZE);
	stdin_info = pmm->alloc(sizeof(ofile_info_t));
	stdin_info->write = invalid_write;
	stdin_info->read = dev_input_read;
	stdin_info->lseek = invalid_lseek;
	stdin_info->count = 1;

	stdout_info = pmm->alloc(sizeof(ofile_info_t));
	stdout_info->write = dev_output_write;
	stdout_info->read = invalid_read;
	stdout_info->lseek = invalid_lseek;
	stdout_info->count = 1;

	stderr_info = pmm->alloc(sizeof(ofile_info_t));
	stderr_info->write = dev_error_write;
	stderr_info->read = invalid_read;
	stderr_info->lseek = invalid_lseek;
	stderr_info->count = 1;
	kmt->sem_init(&fs_lock, "fs lock", 1);
}

static int file_write(ofile_info_t* ofile, int fd, void *buf, int count){
	// TODO: lseek may cause offset beyond the end of the file
	kmt->sem_wait(&fs_lock);
	inode_t inode;
	get_inode_by_no(ofile->inode_no, &inode);
	int left_count = count;
	int inode_blk_idx = ofile->offset / BLK_SIZE;
	int blk_offset = ofile->offset % BLK_SIZE;
	void* cur_buf = buf;
	int file_blk_num = UP_BLK_NUM(inode.size, BLK_SIZE);
	while(left_count){
		int blk_idx;
		if(inode_blk_idx >= file_blk_num){
			blk_idx = alloc_blk();
			insert_blk_into_inode(ofile->inode_no, &inode, blk_idx);
		}else{
			blk_idx = get_blk_idx(inode_blk_idx, &inode);
		}
		Assert(left_count >= 0, "invalid left count %d", left_count);
		int write_count = MIN(BLK_SIZE - blk_offset, left_count);
		sd_write(BLK2ADDR(blk_idx) + blk_offset, cur_buf, write_count);
		cur_buf += write_count;
		left_count -= write_count;
		blk_offset = 0;
		inode_blk_idx ++;
		ofile->offset += write_count;
		inode.size = MAX(ofile->offset, inode.size);
	}
	sd_write(INODE_ADDR(ofile->inode_no) + OFFSET_IN_STRUCT(inode, size), &inode.size, sizeof(int));
	kmt->sem_signal(&fs_lock);
	return count;
}

static int file_read(ofile_info_t* ofile, int fd, void *buf, int count){
	kmt->sem_wait(&fs_lock);
	inode_t inode;
	get_inode_by_no(ofile->inode_no, &inode);
	if(inode.type != FT_FILE && inode.type != FT_DIR){
		printf("file_read: invalid inode type %d inode_no %d\n", inode.type, ofile->inode_no);
		return -1;
	}
	int ret = MIN(inode.size - ofile->offset, count);
	if(ret <= 0){
		kmt->sem_signal(&fs_lock);
		return 0;
	}
	int left_count = ret;
	int inode_blk_idx = ofile->offset / BLK_SIZE;
	int blk_offset = ofile->offset % BLK_SIZE;
	void* cur_buf = buf;
	while(left_count){
		int blk_idx = get_blk_idx(inode_blk_idx, &inode);
		Assert(left_count >= 0, "invalid left count %d", left_count);
		int read_count = MIN(BLK_SIZE - blk_offset, left_count);
		sd_read(BLK2ADDR(blk_idx) + blk_offset, cur_buf, read_count);
		cur_buf += read_count;
		left_count -= read_count;
		blk_offset = 0;
		inode_blk_idx ++;
	}
	ofile->offset += ret;
	kmt->sem_signal(&fs_lock);
	return ret;
}

static int file_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
	inode_t inode;
	switch(whence){
		case SEEK_SET: ofile->offset = offset; break;
		case SEEK_CUR: ofile->offset += offset; break;
		case SEEK_END:
			kmt->sem_wait(&fs_lock);
			get_inode_by_no(ofile->inode_no, &inode);
			kmt->sem_signal(&fs_lock);
			ofile->offset = inode.size; break;
		default: Assert(0, "invalid whence %d", whence);
	}
	return ofile->offset;
}

static int get_inode_by_name(const char* pathname, inode_t* inode, int dirno){
	Assert(strlen(pathname) > 0, "empty string");
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, pathname);
	char* path_name_start = string_buf;
	if(path_name_start[0] == '/') {
		dirno = ROOT_INODE_NO;
		if(!path_name_start[1]){
			get_inode_by_no(dirno, inode);
			return dirno;
		}
		path_name_start = path_name_start + 1;
	}
	char* token = path_name_start;
	int delim_idx = find_replace(path_name_start, "/", 0);
	int inode_no = dirno;
	get_inode_by_no(dirno, inode);
	while(token){
		inode_no = search_inodeno_in_dir(inode, token);
		if(inode_no <= -1){
			return -1;
		}
		get_inode_by_no(inode_no, inode);
		token = delim_idx == -1 ? NULL : path_name_start + delim_idx + 1;
		delim_idx = find_replace(path_name_start, "/", delim_idx + 1);
	}
	return inode_no;
}

static int vfs_write(int fd, void *buf, int count){
	task_t* cur_task = kmt->gettask();
	if(!IS_VALID_FD(fd) || !cur_task->ofiles[fd]){
		printf("write: invalid fd %d\n", fd);
		return -1;
	}
	return cur_task->ofiles[fd]->write(cur_task->ofiles[fd], fd, buf, count);
}

static int vfs_read(int fd, void *buf, int count){
	task_t* cur_task = kmt->gettask();
	if(!IS_VALID_FD(fd) || !cur_task->ofiles[fd]){
		printf("read: invalid fd %d\n", fd);
		return -1;
	}
	return cur_task->ofiles[fd]->read(cur_task->ofiles[fd], fd, buf, count);
}

static int vfs_close(int fd){
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

static proc_inode_t* search_inode_in_proc_dir(proc_inode_t* parent, char* token){
	int entry_num = parent->size / sizeof(proc_diren_t);
	for(int i = 0; i < entry_num; i++){
		proc_diren_t* proc_dirent = (proc_diren_t*)parent->mem + i;
		if(strcmp(token, proc_dirent->name) == 0){
			return proc_dirent->inode;
		}
	}
	return NULL;
}

static int fill_task_ofile(ofile_info_t* ofile){
	task_t* cur_task = kmt->gettask();
	for(int i = STDERR_FILENO + 1; i < MAX_OPEN_FILE; i++){
		if(!cur_task->ofiles[i]){
			cur_task->ofiles[i] = ofile;
			return i;
		}
	}
	Assert(0, "number of opening files is more than %d", MAX_OPEN_FILE);
}

static int proc_open(const char* pathname, int flags){
	if(pathname[0] == '/') pathname = pathname + 1;
	kmt->sem_wait(&procfs_lock);
	proc_inode_t* parent = proc_dir;
	if(!pathname || !pathname[0]){

	}else{
		char string_buf[MAX_STRING_BUF_LEN];
		strcpy(string_buf, pathname);
		char* token = string_buf;
		int idx = find_replace(string_buf, "/", 0);
		while(token){
			parent = search_inode_in_proc_dir(parent, token);
			if(!parent){
				printf("/proc/%s not found\n", pathname);
				kmt->sem_signal(&procfs_lock);
				return -1;
			}
			token = idx == -1 ? NULL : string_buf + idx + 1;
			idx = find_replace(string_buf, "/", idx + 1);
		}
	}
	kmt->sem_signal(&procfs_lock);
	ofile_info_t* tmp_ofile = pmm->alloc(sizeof(ofile_info_t));
	tmp_ofile->read = proc_read;
	tmp_ofile->write = invalid_write;
	tmp_ofile->lseek = proc_lseek;
	tmp_ofile->proc_inode = parent;
	tmp_ofile->type = CWD_PROCFS;
	tmp_ofile->flag = flags;
	tmp_ofile->offset = 0;
	tmp_ofile->count = 1;
	return fill_task_ofile(tmp_ofile);
}

static dev_inode_t* find_dev(const char* pathname){
	if(!pathname || (strlen(pathname) > DEV_NAME_LEN)) return NULL;
	for(int i = 0; i < dev_num; i++){
		dev_inode_t* select = dev_start + i;
		if(strcmp(pathname, select->name) == 0) return select;
	}
	return NULL;
}

static int dev_open(const char* pathname, int flags){
	if(pathname[0] != '/'){
		printf("/dev/%s not found\n", pathname);
		return -1;
	}
	pathname = pathname + 1;
	dev_inode_t* dev_inode = find_dev(pathname);
	if(!dev_inode){
		printf("/dev/%s not found\n", pathname);
		return -1;
	}

	ofile_info_t* tmp_ofile = pmm->alloc(sizeof(ofile_info_t));
	tmp_ofile->read = dev_inode->read;
	tmp_ofile->write = dev_inode->write;
	tmp_ofile->lseek = dev_inode->lseek;
	tmp_ofile->dev_inode = dev_inode;
	tmp_ofile->type = CWD_DEVFS;
	tmp_ofile->flag = flags;
	tmp_ofile->offset = 0;
	tmp_ofile->count = 1;
	return fill_task_ofile(tmp_ofile);
}

static int vfs_open(const char *pathname, int flags){
	// traverse inode block
	int pathname_len = strlen(pathname);
	if(pathname_len <= 0 || pathname_len >= MAX_STRING_BUF_LEN){
		printf("open: invalid pathname length %s %d\n", pathname, pathname_len);
		return -1;
	}
	/* /proc, /dev can only be accessed by pathname started with '/' */
	if(strncmp(pathname, "/proc", 5) == 0) return proc_open(pathname + 5, flags);
	if(strncmp(pathname, "/dev", 4) == 0) return dev_open(pathname + 4, flags);

	kmt->sem_wait(&fs_lock);
	int root_inode_no = pathname[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, pathname);
	int name_idx = split_base_name(string_buf);;
	inode_t dir_inode;
	int dir_inode_no = name_idx <= 0? root_inode_no : get_inode_by_name(string_buf, &dir_inode, root_inode_no);
	if(dir_inode_no < 0){
		printf("open: no such file or directory %s\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	inode_t file_inode;
	int file_inode_no = string_buf[name_idx + 1] == 0 ? dir_inode_no : get_inode_by_name(string_buf + name_idx + 1, &file_inode, dir_inode_no);
	if(file_inode_no < 0 && (flags & O_CREAT) && ((dir_inode_no == root_inode_no) || (dir_inode.type == FT_DIR))){
		file_inode_no = alloc_inode(FT_FILE, &file_inode);
		if(file_inode_no < 0){
			kmt->sem_signal(&fs_lock);
			printf("open: no such file or directory %s\n", pathname);
			return -1;
		}
		insert_into_dir(dir_inode_no, file_inode_no, string_buf + name_idx + 1);
	}
	if(file_inode_no < 0){
		printf("open: no such file or directory %s\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	while(file_inode.type == FT_LINK) {
		file_inode_no = link_inodeno_by_inode(&file_inode);
		get_inode_by_no(file_inode_no, &file_inode);
	}
	int flag_mode = flags & 3;
	int writable = (flag_mode == O_WRONLY) || (flag_mode == O_RDWR);
	int readable =  (flag_mode == O_RDONLY) || (flag_mode == O_RDWR);
	if(file_inode.type == FT_DIR && writable){
		printf("dir %s is not writable\n");
		kmt->sem_signal(&fs_lock);
		return -1;
	}

	ofile_info_t* tmp_ofile = pmm->alloc(sizeof(ofile_info_t));
	tmp_ofile->write = writable ? file_write : invalid_write;
	tmp_ofile->read = readable ? file_read : invalid_read;
	tmp_ofile->lseek = file_lseek;
	tmp_ofile->offset = 0;
	tmp_ofile->count = 1;
	tmp_ofile->inode_no = file_inode_no;
	tmp_ofile->type =CWD_UFS;
	tmp_ofile->flag = flags;

	kmt->sem_signal(&fs_lock);
	return fill_task_ofile(tmp_ofile);
}

static int vfs_lseek(int fd, int offset, int whence){
	task_t* cur_task = kmt->gettask();
	if(!IS_VALID_FD(fd) || !cur_task->ofiles[fd]){
		printf("lseek: invalid fd %d\n", fd);
		return -1;
	}
	return cur_task->ofiles[fd]->lseek(cur_task->ofiles[fd], fd, offset, whence);
}

static int vfs_link(const char *oldpath, const char *newpath){
	int oldpath_len = strlen(oldpath);
	if(oldpath_len <= 0 || oldpath_len >= MAX_STRING_BUF_LEN){
		printf("link: invalid oldpath length %s %d\n", oldpath, oldpath_len);
		return -1;
	}
	int newpath_len = strlen(newpath);
	if(newpath_len <= 0 || newpath_len >= MAX_STRING_BUF_LEN){
		printf("link: invalid newpath length %s %d\n", newpath, newpath_len);
		return -1;
	}
	int old_root_inode_no = oldpath[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;

	kmt->sem_wait(&fs_lock);
	inode_t old_inode;
	int old_inode_no = get_inode_by_name(oldpath, &old_inode, old_root_inode_no);
	if(old_inode_no < 0){
		printf("link: no such file or directory %s\n", oldpath);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	if(old_inode.type == FT_DIR){
		printf("link: hard link not allowed for directory %s\n", oldpath);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, newpath);
	int name_idx = split_base_name(string_buf);
	Assert(strlen(string_buf + name_idx + 1) > 0, "newpath is not a file %s", newpath);
	int new_root_inode_no = newpath[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	inode_t new_inode;
	int dir_inode_no = name_idx <= 0 ? new_root_inode_no : get_inode_by_name(string_buf, &new_inode, new_root_inode_no);
	if(dir_inode_no < 0){
		printf("link: no such file or directory %s\n", newpath);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	int new_inode_no = get_inode_by_name(string_buf + name_idx + 1, &new_inode, dir_inode_no);
	if(new_inode_no >= 0){
		printf("link: file %s exists\n", newpath);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	new_inode_no = alloc_inode(FT_LINK, &new_inode);
	if(new_inode_no < 0){
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	insert_into_dir(dir_inode_no, new_inode_no, string_buf + name_idx + 1);
	new_inode.link_no = old_inode_no;
	sd_write(INODE_ADDR(new_inode_no) + OFFSET_IN_STRUCT(new_inode, link_no), &new_inode.link_no, sizeof(int));

	old_inode.n_link ++;
	sd_write(INODE_ADDR(old_inode_no) + OFFSET_IN_STRUCT(old_inode, n_link), &old_inode.n_link, sizeof(int));
	kmt->sem_signal(&fs_lock);
	return 0;
}

static void replace_dirent_by_idx(inode_t* inode, int idx, diren_t* new_diren){
	int blk_idx = idx * sizeof(diren_t) / BLK_SIZE;
	int blk_offset = (idx * sizeof(diren_t)) % BLK_SIZE;
	int left_size = sizeof(diren_t);
	while(left_size){
		int write_size = MIN(left_size, BLK_SIZE - blk_offset);
		sd_write(BLK2ADDR(get_blk_idx(blk_idx, inode)) + blk_offset, (void*)new_diren + sizeof(diren_t) - left_size, write_size);
		left_size -= write_size;
		blk_offset = 0;
		blk_idx += 1;
	}
}

static inline void remove_dirent_from_inode(inode_t* inode, diren_t* diren, int diren_idx){
	int entry_num = inode->size / sizeof(diren_t);
	int pre_blkidx = (inode->size - 1) / BLK_SIZE;
	if(diren_idx == (entry_num - 1)){ // last dirent of inode, nothiong else to do
	}else{
		diren_t last_diren;
		get_dirent_by_idx(inode, entry_num - 1, &last_diren);
		replace_dirent_by_idx(inode, diren_idx, &last_diren);
	}
	inode->size -= sizeof(diren_t);
	int new_blkidx = (inode->size - 1) / BLK_SIZE;
	if(pre_blkidx != new_blkidx){
		int blk_no = get_blk_idx(pre_blkidx, inode);
		free_blk(blk_no);
	}

}

static void remove_inode_from_parent(int dir_inode_no, int delete_no){
	inode_t dir_inode;
	get_inode_by_no(dir_inode_no, &dir_inode);
	Assert(dir_inode.type == FT_DIR, "inode %d is not a dir", dir_inode_no);
	Assert(dir_inode.size % sizeof(diren_t) == 0, "invalid inode size %d\n", dir_inode.size);
	int entry_num = dir_inode.size / sizeof(diren_t);
	diren_t diren;
	for(int i = 0; i < entry_num; i++){
		get_dirent_by_idx(&dir_inode, i, &diren);
		if(diren.inode_idx == delete_no){
			remove_dirent_from_inode(&dir_inode, &diren, i);
			sd_write(INODE_ADDR(dir_inode_no), &dir_inode, sizeof(inode_t));
			return;
		}
	}
	Assert(0, "delete no %d is not in dir %d\n", delete_no, dir_inode_no);
}

static int vfs_unlink(const char *pathname){
	int path_len = strlen(pathname);
	Assert(path_len > 0 && path_len < MAX_STRING_BUF_LEN, "invalid string length %d", path_len);
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, pathname);
	int name_idx = split_base_name(string_buf);
	Assert(strlen(string_buf + name_idx + 1) > 0, "pathname is not a file %s", pathname);
	int root_inode_no = pathname[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	kmt->sem_wait(&fs_lock);
	inode_t dir_inode;
	int dir_inode_no = name_idx <= 0? root_inode_no : get_inode_by_name(string_buf, &dir_inode, root_inode_no);
	inode_t delete_inode;
	int delete_no = get_inode_by_name(pathname, &delete_inode, root_inode_no);
	if(delete_no < 0){
		printf("unlink: no such file or directory %s\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}

	if(delete_inode.type == FT_DIR && delete_inode.size > 2 * sizeof(diren_t)){
		printf("can not unlink a non-empty dir %s\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}

	if(delete_inode.type == FT_LINK){
		int link_no = link_inodeno_by_inode(&delete_inode);
		inode_t origin_inode;
		get_inode_by_no(link_no, &origin_inode);
		origin_inode.n_link --;
		if(origin_inode.n_link == 0){
			remove_inode_blk_by_inode(&origin_inode);
			free_inode(link_no);
		}else{
			sd_write(INODE_ADDR(link_no) + OFFSET_IN_STRUCT(origin_inode, n_link), &origin_inode.n_link, sizeof(int));
		}
	}

	if(dir_inode_no == delete_no){
		printf("unlink: refuse to unlink .\n");
		kmt->sem_signal(&fs_lock);
		return -1;
	}

	remove_inode_from_parent(dir_inode_no, delete_no);
	delete_inode.n_link --;
	if(delete_inode.n_link == 0){
		remove_inode_blk_by_inode(&delete_inode);
		free_inode(delete_no);
	}else{
		sd_write(INODE_ADDR(delete_no) + OFFSET_IN_STRUCT(delete_inode, n_link), &delete_inode.n_link, sizeof(int));
	}
	kmt->sem_signal(&fs_lock);
	return 0;
}

static int vfs_fstat(int fd, struct ufs_stat *buf){
	task_t* cur_task = kmt->gettask();
	if(!cur_task->ofiles[fd]){
		printf("fstat: invalid fd %d\n", fd);
		return -1;
	}
	inode_t inode;
	int inode_no = cur_task->ofiles[fd]->inode_no;
	kmt->sem_wait(&fs_lock);
	get_inode_by_no(inode_no, &inode);
	kmt->sem_signal(&fs_lock);
	if(inode_no < 0){
		printf("fstat: invalid fd %d\n", fd);
		return -1;
	}
	buf->id = fd;
	buf->type = inode.type;
	buf->size = inode.size;
	return 0;
}

static int vfs_mkdir(const char *pathname){
	int path_len = strlen(pathname);
	if(path_len <= 0 || path_len >= MAX_STRING_BUF_LEN){
		printf("mkdir: invalid pathname length %s %d\n", pathname, path_len);
		return -1;
	}
	int root_inode_no = pathname[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, pathname);
	int name_idx = split_base_name(string_buf);
	Assert(strlen(string_buf + name_idx + 1) > 0, "%s is not a file", pathname);

	kmt->sem_wait(&fs_lock);
	inode_t new_inode;
	int dir_inode_no;
	if(name_idx <= 0){
		dir_inode_no = root_inode_no;
		get_inode_by_no(root_inode_no, &new_inode);
	}else{
		dir_inode_no = get_inode_by_name(string_buf, &new_inode, root_inode_no);
	}
	if(dir_inode_no < 0){
		printf("mkdir: no such file or directory %s\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	if((dir_inode_no != 0) && (new_inode.type != FT_DIR)){
		printf("mkdir: %s is not a dir\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	if(string_buf[name_idx + 1] == 0){
		printf("mkdir: %s is not valid\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	inode_t file_inode;
	int file_inode_no = get_inode_by_name(string_buf + name_idx + 1, &file_inode, dir_inode_no);
	if(file_inode_no >= 0){
		printf("mkdir: %s is already exists\n", pathname);
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	int new_inode_no = alloc_inode(FT_DIR, &new_inode);
	if(new_inode_no < 0){
		kmt->sem_signal(&fs_lock);
		return -1;
	}
	insert_into_dir(dir_inode_no, new_inode_no, string_buf + name_idx + 1);
	insert_into_dir(new_inode_no, new_inode_no, ".");
	insert_into_dir(new_inode_no, dir_inode_no, "..");
	kmt->sem_signal(&fs_lock);
	return 0;
}

static int vfs_chdir(const char *path){
	int path_len = strlen(path);
	if(path_len <= 0 || path_len >= MAX_STRING_BUF_LEN){
		printf("chdir: invalid path length %s %d\n", path, path_len);
		return -1;
	}

	task_t* task = kmt->gettask();

	kmt->sem_wait(&fs_lock);
	inode_t inode;
	int inode_no = get_inode_by_name(path, &inode, task->cwd_inode_no);
	kmt->sem_signal(&fs_lock);

	if(inode_no < 0){
		printf("chdir: no such file or directory %s\n", path);
		return -1;
	}
	if(inode.type != FT_DIR){
		printf("%s is not a dir\n", path);
		return -1;
	}
	task->cwd_inode_no = inode_no;
	return 0;
}

static int vfs_dup(int fd){
	task_t* task = kmt->gettask();
	if(fd >= MAX_OPEN_FILE || !task->ofiles[fd]){
		printf("dup: invalid fd %d\n", fd);
		return -1;
	}
	for(int i = 0; i < MAX_OPEN_FILE; i++){
		if(!task->ofiles[i]){
			task->ofiles[i] = task->ofiles[fd];
			task->ofiles[i]->count ++;
			return i;
		}
	}
	printf("dup: ofiles full\n");
	return -1;
}

ofile_info_t* filedup(ofile_info_t* ofile){
	// TODO: lock
	ofile->count ++;
	return ofile;
}

void fileclose(ofile_info_t* ofile){
	// TODO: lock
	Assert(ofile->count > 0, "invalid ofile count %d\n", ofile->count);
	if(--ofile->count > 0){
		return;
	}
	pmm->free(ofile);
}

MODULE_DEF(vfs) = {
	.init   = vfs_init,
	.write  = vfs_write,
	.read   = vfs_read,
	.close  = vfs_close,
	.open   = vfs_open,
	.lseek  = vfs_lseek,
	.link   = vfs_link,
	.unlink = vfs_unlink,
	.fstat  = vfs_fstat,
	.mkdir  = vfs_mkdir,
	.chdir  = vfs_chdir,
	.dup    = vfs_dup,
};

#ifdef VFS_DEBUG

void run_fs_test() {
#include "../test/vfs-workload.inc"
}

void traverse(const char *root) {
  char *buf = pmm->alloc(BLK_SIZE); // asserts success
  struct ufs_stat s;

  int fd = vfs->open(strcmp(root, "") == 0 ? "/" : root, O_RDONLY), nread;
  if (fd < 0) goto release;

  vfs->fstat(fd, &s);
  if (s.type == FT_DIR) {
    while ( (nread = vfs->read(fd, buf, BLK_SIZE)) > 0) {
      for (int offset = 0;
          offset +  sizeof(diren_t) <= nread;
          offset += sizeof(diren_t)) {
        diren_t *d = (diren_t *)(buf + offset);
        if (d->name[0] != '.') { // 小彩蛋：你这下知道为什么
                                 // Linux 以 “.” 开头的文件是隐藏文件了吧
          char *fname = pmm->alloc(MAX_STRING_BUF_LEN); // assert success
          sprintf(fname, "%s/%s", root, d->name);
          traverse(fname);
          pmm->free(fname);
        }
      }
    }
  }

release:
  if (fd >= 0) vfs->close(fd);
  pmm->free(buf);
}

void vfs_readFileList(int root_idx, int depth){
	inode_t root_inode;
	sd_read(INODE_ADDR(root_idx), &root_inode, sizeof(inode_t));
	if(root_inode.type != FT_DIR) return;
  int entry_num = root_inode.size / sizeof(diren_t);

  int entry_idx = 0;
	diren_t diren;
	for(entry_idx = 0; entry_idx < entry_num; entry_idx ++){
		get_dirent_by_idx(&root_inode, entry_idx, &diren);
		if(diren.type == DIRENT_SINGLE){
			for(int i = 0; i < depth; i++) printf("  ");
			printf("[%d]%s\n", entry_idx, diren.name);
			if((strcmp(diren.name, ".") == 0) || (strcmp(diren.name, "..") == 0)) continue;
		}else if(diren.type == DIRENT_START){
			for(int i = 0; i < depth; i++) printf("  ");
			printf("[%d]", entry_idx);
      while(diren.type != DIRENT_END){
        printf("%s", diren.name);
        get_dirent_by_idx(&root_inode, diren.next_entry, &diren);
      }
      printf("%s\n", diren.name);
		}
		vfs_readFileList(diren.inode_idx, depth + 1);
	}
}


#endif
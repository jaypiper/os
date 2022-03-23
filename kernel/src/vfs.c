#include <common.h>
#include <kmt.h>
#include <vfs.h>
#include <user.h>

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

static int invalid_write(ofile_info_t* ofile, int fd, void *buf, int count){
	Assert(0, "invalid write for fd %d", fd);
}

static int invalid_read(ofile_info_t* ofile, int fd, void *buf, int count){
	Assert(0, "invalid read for fd %d", fd);
}

static int invalid_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
	Assert(0, "invalid lseek for fd %d", fd);
}

static void get_inode_by_no(int inode_no, inode_t* inode){
	sd_read(sb->inode_start * BLK_SIZE + inode_no * sizeof(inode_t), inode, sizeof(inode_t));
}

/* get the disk block idx for a given block idx in inode*/
static int get_blk_idx(int idx, inode_t* inode){
  if(idx < MAX_DIRECT_FILE_BLOCK) return inode->addr[idx];
  idx -= MAX_DIRECT_FILE_BLOCK;
  Assert(idx < (MAX_DIRECT_FILE_BLOCK + inode->addr[DEPTH_IN_INODE] * INDIRECT_NUM_PER_BLK), "idx %d is out of dir with depth %d", idx, inode->addr[DEPTH_IN_INODE]);
  int blk_start = BLK2ADDR(inode->addr[INDIRECT_IN_INODE]);
  int depth = UP_BLK_NUM(idx, INDIRECT_NUM_PER_BLK);
  for(int i = 0; i < depth - 1; i++){
		uint32_t next_blk;
		sd_read(blk_start + INDIRECT_NUM_PER_BLK * sizeof(uint32_t), &next_blk, sizeof(uint32_t));
		blk_start = BLK2ADDR(next_blk);
		// blk_start = sd_read(BLK2ADDR(blk_start[INDIRECT_NUM_PER_BLK]), );
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
		sd_read(bitmap_blk_start + search_idx / 8, &bitmap, sizeof(uint32_t));
		if(bitmap == MAX32bit) continue;
		int free_bit = __builtin_ctz(~bitmap);
		bitmap = bitmap | ((uint32_t)1 << free_bit);
		sd_write(bitmap_blk_start + search_idx / 8, &bitmap, sizeof(uint32_t));
		return search_idx + free_bit;
	}
	printf("alloc: no avaliable block\n");
	return -1;
}

static int free_blk(int blk_no){
	int bitmap_blk_start = BLK2ADDR(sb->bitmap_start);
	uint32_t bitmap;
	sd_read(bitmap_blk_start + blk_no / 8, &bitmap, sizeof(uint32_t));
	Assert(bitmap & (uint32_t)1 << (blk_no & MAX32bit), "blk %d is not allocated", blk_no);
	bitmap = bitmap & (~((uint32_t)1 << (blk_no & MAX32bit)));
	sd_write(bitmap_blk_start + blk_no / 8, &bitmap, sizeof(uint32_t));
	return 0;
}

static int alloc_inode(int type, inode_t* inode){
	int inode_blk_start = BLK2ADDR(sb->inode_start);
	int inode_start = inode_blk_start;
	for(int inode_no = 0; inode_no < N_INODE; inode_no ++){
		sd_read(inode_start, inode, sizeof(inode_t));
		// printf("type %d\n", inode->type );
		if(inode->type == FT_UNUSED){
			inode->type = type;
			inode->n_link = 0;
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

static int link_inodeno_by_inode(inode_t* inode){
	Assert(inode->type == FT_LINK, "inode is not a link %d", inode->type);
	link_t inode_link;
	sd_read(BLK2ADDR(inode->addr[0]), &inode_link, sizeof(inode_link));
	return inode_link.inode_no;
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
    int idx = insert_idx - MAX_DIRECT_FILE_BLOCK;
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
	}
}

static int search_inodeno_in_dir(inode_t* dir_inode, char* name){
	int entry_num = dir_inode->size / sizeof(diren_t);
	int left_entry_num = entry_num;
	int blk_idx = 0, total_blk_num = UP_BLK_NUM(entry_num, DIREN_PER_BLOCK);
	int blk_start = dir_inode->addr[blk_idx];
	diren_t direntry;
	for(blk_idx = 0; blk_idx < total_blk_num; blk_idx ++){
		blk_start = BLK2ADDR(get_blk_idx(blk_idx, dir_inode));
		int dirent_addr = blk_start;
		for(int i = 0; i < MIN(left_entry_num, DIREN_PER_BLOCK); i++){
			sd_read(dirent_addr, &direntry, sizeof(diren_t));
			if(direntry.type == DIRENT_SINGLE){
				if(!strncmp(direntry.name, name, DIREN_NAME_LEN)){
					return direntry.inode_idx;
				}
			}else if(direntry.type == DIRENT_START){
				char* match_name = name;
				while(!strncmp(direntry.name, match_name, DIREN_NAME_LEN)){
					if(direntry.type == DIRENT_END) return direntry.inode_idx;
					get_dirent_by_idx(dir_inode, direntry.next_entry, &direntry);
					match_name = name + DIREN_NAME_LEN;
				}
			}
			dirent_addr += sizeof(diren_t);
		}
		left_entry_num = MAX(0, left_entry_num - DIREN_PER_BLOCK);
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
    // memcpy(dirent_blk_start + offset, insert_pos, insert_size);
    insert_pos += insert_size;
    left_size -= insert_size;
    offset = 0;
  }
  inode->size += sizeof(diren_t);
	sd_write(INODE_ADDR(inode_no) + OFFSET_IN_PSTRUCT(inode, size), &inode->size, sizeof(int));
}

static void insert_into_dir(int parent_inode, int child_inode, char* name){
	inode_t parent;
	sd_read(INODE_ADDR(parent_inode), &parent, sizeof(inode_t));
  Assert(parent.size % sizeof(diren_t) == 0, "size 0x%x dirent 0x%lx", parent.size, sizeof(diren_t));
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
	// TODO: create /proc, /dev
	// TODO: recover from log
	// TODO: stdin/stdout/stderr should in task_t
	dev_sd = dev->lookup("sda");
	sd_op->init(dev_sd);
	sb = pmm->alloc(BLK_SIZE);
	log = (fslog_t*)(sb + 1);
	sd_read(FS_START, sb, BLK_SIZE);
	stdin_info = pmm->alloc(sizeof(ofile_info_t));
	stdin_info->write = invalid_write;
	stdin_info->read = dev_input_read;
	stdin_info->lseek = invalid_lseek;
	stdout_info = pmm->alloc(sizeof(ofile_info_t));
	stdout_info->write = dev_output_write;
	stdout_info->read = invalid_read;
	stdout_info->lseek = invalid_lseek;
	stderr_info = pmm->alloc(sizeof(ofile_info_t));
	stdout_info->write = dev_error_write;
	stdout_info->read = invalid_read;
	stdout_info->lseek = invalid_lseek;
}

static int file_write(ofile_info_t* ofile, int fd, void *buf, int count){
	// TODO: lseek may cause offset beyond the end of the file
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
	}
	return count;
}

static int file_read(ofile_info_t* ofile, int fd, void *buf, int count){
	inode_t inode;
	get_inode_by_no(ofile->inode_no, &inode);
	int ret = MIN(inode.size - ofile->offset, count);
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
	return ret;
}

static int file_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
	inode_t inode;
	switch(whence){
		case SEEK_SET: ofile->offset = offset; break;
		case SEEK_CUR: ofile->offset += offset; break;
		case SEEK_END:
			get_inode_by_no(ofile->inode_no, &inode);
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
		path_name_start = path_name_start + 1;
		if(!path_name_start[1]) return dirno;
	}
	char *token = strtok(path_name_start, "/");
	int inode_no = dirno;
	get_inode_by_no(dirno, inode);
	while(token){
		inode_no = search_inodeno_in_dir(inode, token);
		if(inode_no <= -1){
			return -1;
		}
		get_inode_by_no(inode_no, inode);
		token = strtok(NULL, "/");
	}
	return inode_no;
}

static int vfs_write(int fd, void *buf, int count){
	task_t* cur_task = kmt->gettask();
	return cur_task->ofiles[fd]->write(cur_task->ofiles[fd], fd, buf, count);
}

static int vfs_read(int fd, void *buf, int count){
	task_t* cur_task = kmt->gettask();
	return cur_task->ofiles[fd]->read(cur_task->ofiles[fd], fd, buf, count);
}

static int vfs_close(int fd){
	task_t* cur_task = kmt->gettask();
	if(!cur_task->ofiles[fd]){
		printf("file %d is not open\n", fd);
		return -1;
	}
	pmm->free(cur_task->ofiles[fd]);
	cur_task->ofiles[fd] = NULL;
	return 0;
}

static int vfs_open(const char *pathname, int flags){  // must start with /
	// traverse inode block
	int pathname_len = strlen(pathname);
	Assert(pathname_len > 0 && pathname_len < MAX_STRING_BUF_LEN, "invalid pathname_len %d: %s", pathname_len, pathname);
	int root_inode_no = pathname[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, pathname);
	int name_idx;
	for(name_idx = strlen(pathname) - 1; name_idx >= 0; name_idx--){
		if(string_buf[name_idx] == '/') {
			string_buf[name_idx] = 0;
			break;
		}
	}
	Assert(strlen(string_buf + name_idx + 1) > 0, "pathname is not a file %s", pathname);
	inode_t dir_inode;
	int dir_inode_no = get_inode_by_name(string_buf, &dir_inode, root_inode_no);
	if(dir_inode_no < 0) return -1;
	inode_t file_inode;
	int file_inode_no = get_inode_by_name(string_buf + name_idx + 1, &file_inode, dir_inode_no);
	if(file_inode_no < 0 && (flags & O_CREAT)){
		file_inode_no = alloc_inode(FT_FILE, &file_inode);
		insert_into_dir(dir_inode_no, file_inode_no, string_buf + name_idx);
	}
	if(file_inode_no < 0){
		printf("no such file or directory %s\n", pathname);
	}
	task_t* cur_task = kmt->gettask();
	for(int i = STDERR_FILENO + 1; i < MAX_OPEN_FILE; i++){
		if(!cur_task->ofiles[i]){
			ofile_info_t* tmp_ofile = pmm->alloc(sizeof(ofile_info_t));
			cur_task->ofiles[i] = tmp_ofile;
			tmp_ofile->write = file_write;
			tmp_ofile->read = file_read;
			tmp_ofile->lseek = file_lseek;
			tmp_ofile->offset = 0;
			tmp_ofile->inode_no = file_inode_no;
			tmp_ofile->flag = flags;
			return i;
		}
	}
	Assert(0, "number of opening files is more than %d", MAX_OPEN_FILE);
}

static int vfs_lseek(int fd, int offset, int whence){
	task_t* cur_task = kmt->gettask();
	return cur_task->ofiles[fd]->lseek(cur_task->ofiles[fd], fd, offset, whence);
}

static int vfs_link(const char *oldpath, const char *newpath){
	int oldpath_len = strlen(oldpath);
	Assert(oldpath_len > 0 && oldpath_len < MAX_STRING_BUF_LEN, "invalid string length %d", oldpath_len);
	Assert(oldpath[0] == '/', "file %s is not starting with /", oldpath);
	int root_inode_no = oldpath[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	inode_t old_inode;
	int old_inode_no = get_inode_by_name(oldpath, &old_inode, root_inode_no);
	if(old_inode_no < 0){
		printf("no such file or directory %s\n", oldpath);
		return -1;
	}
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, newpath);
	int name_idx;
	for(name_idx = strlen(newpath) - 1; name_idx >= 0; name_idx--){
		if(string_buf[name_idx] == '/') {
			string_buf[name_idx] = 0;
			break;
		}
	}
	Assert(name_idx >= 0, "name_idx=%d", name_idx);
	Assert(strlen(string_buf + name_idx + 1) > 0, "newpath is not a file %s", newpath);
	inode_t new_inode;
	int dir_inode_no = name_idx == 0 ? 0 : get_inode_by_name(string_buf, &new_inode, ROOT_INODE_NO);
	if(dir_inode_no < 0){
		printf("no such file or directory %s\n", newpath);
		return -1;
	}

	int new_inode_no = alloc_inode(FT_LINK, &new_inode);
	insert_into_dir(dir_inode_no, new_inode_no, string_buf + name_idx + 1);
	int new_blk_no = alloc_blk();
	insert_blk_into_inode(new_inode_no, &new_inode, new_blk_no);
	link_t new_link = {.inode_no = old_inode_no};
	sd_write(BLK2ADDR(new_blk_no), &new_link, sizeof(link_t));
	old_inode.n_link ++;
	sd_write(INODE_ADDR(old_inode_no) + OFFSET_IN_STRUCT(old_inode, n_link), &old_inode.n_link, sizeof(int));
	return 0;
}

static int vfs_unlink(const char *pathname){
	int path_len = strlen(pathname);
	Assert(path_len > 0 && path_len < MAX_STRING_BUF_LEN, "invalid string length %d", path_len);
	int root_inode_no = pathname[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	inode_t delete_inode;
	int delete_no = get_inode_by_name(pathname, &delete_inode, root_inode_no);
	if(delete_no < 0){
		printf("no such file or directory %s\n", pathname);
		return -1;
	}
	int link_no = link_inodeno_by_inode(&delete_inode);
	remove_inode_blk_by_inode(&delete_inode);
	free_inode(delete_no);
	inode_t origin_inode;
	get_inode_by_no(link_no, &origin_inode);
	origin_inode.n_link --;
	if(origin_inode.n_link == 0){
		remove_inode_blk_by_inode(&origin_inode);
		free_inode(link_no);
	}else{
		sd_write(INODE_ADDR(link_no) + OFFSET_IN_STRUCT(origin_inode, n_link), &origin_inode.n_link, sizeof(int));
	}
	return 0;
}

static int vfs_fstat(int fd, struct ufs_stat *buf){
	inode_t inode;
	int inode_no = kmt->gettask()->ofiles[fd]->inode_no;
	get_inode_by_no(inode_no, &inode);
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
	Assert(path_len > 0 && path_len < MAX_STRING_BUF_LEN, "invalid string length %d", path_len);
	int root_inode_no = pathname[0] == '/' ? ROOT_INODE_NO : kmt->gettask()->cwd_inode_no;
	char string_buf[MAX_STRING_BUF_LEN];
	strcpy(string_buf, pathname);
	int name_idx;
	for(name_idx = strlen(pathname) - 1; name_idx >= 0; name_idx--){
		if(string_buf[name_idx] == '/') {
			string_buf[name_idx] = 0;
			break;
		}
	}
	Assert(name_idx >= 0, "name_idx=%d", name_idx);
	Assert(strlen(string_buf + name_idx + 1) > 0, "%s is not a file", pathname);
	inode_t new_inode;
	int dir_inode_no = name_idx == 0 ? 0 : get_inode_by_name(string_buf, &new_inode, root_inode_no);
	if(dir_inode_no < 0){
		printf("no such file or directory %s\n", pathname);
		return -1;
	}

	int new_inode_no = alloc_inode(FT_DIR, &new_inode);
	insert_into_dir(dir_inode_no, new_inode_no, string_buf + name_idx + 1);
	insert_into_dir(new_inode_no, new_inode_no, ".");
	insert_into_dir(new_inode_no, dir_inode_no, "..");
	return 0;
}

static int vfs_chdir(const char *path){
	int path_len = strlen(path);
	Assert(path_len > 0 && path_len < MAX_STRING_BUF_LEN, "invalid string length %d", path_len);

	task_t* task = kmt->gettask();
	inode_t inode;
	int inode_no = get_inode_by_name(path, &inode, task->cwd_inode_no);
	if(inode_no < 0){
		printf("no such file or directory %s\n", path);
		return -1;
	}
	task->cwd_inode_no = inode_no;
	return 0;
}

static int vfs_dup(int fd){
	task_t* task = kmt->gettask();
	if(fd >= MAX_OPEN_FILE || !task->ofiles[fd]){
		printf("invalid fd %d\n", fd);
		return -1;
	}
	for(int i = 0; i < MAX_OPEN_FILE; i++){
		if(!task->ofiles[i]){
			task->ofiles[i] = task->ofiles[fd];
			return i;
		}
	}
	printf("ofiles full\n");
	return -1;
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
    while ( (nread = vfs->read(fd, buf, sizeof(diren_t))) > 0) {
      for (int offset = 0;
          offset +  sizeof(struct ufs_dirent) <= nread;
          offset += sizeof(struct ufs_dirent)) {
        struct ufs_dirent *d = (struct ufs_dirent *)(buf + offset);
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

#endif
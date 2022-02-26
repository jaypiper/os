#include <common.h>
#include <pmm.h>

uintptr_t pmsize;
static void* pheap_start;
static void* pheap_end;
static void* bstart;
static bentry_t* buddy_tree;
static int leaf_base;
lock_t global_lock;

slab_t slab[MAX_CPU][SLAB_NUM];       // 4B - 1KB (PGSIZE)
lock_t cpu_lock[MAX_CPU];
bentry_t* buddy = NULL;

static inline size_t next_pow2(size_t size){
  int bits = sizeof(unsigned long)*8 - __builtin_clzl(size-1);
  return size == 1 ? 1 : 1 << bits; 
}

static inline void update_buddy_alloc(int select){
  Assert(select >= 0 && select <= leaf_base * 2, "select %d not in [0, %d]\n", select, leaf_base*2);
  buddy_tree[select].type = BLOCK_ALLOCATED;
  buddy_tree[select].max_pgnum = buddy_tree[select].pgnum = 0;
  while(!IS_ROOT(select)){
    select = PARENT(select);
    Assert((buddy_tree[select].max_pgnum & (next_pow2(buddy_tree[select].max_pgnum) - 1)) == 0, "invalid max_pgnum, select %d max_pgnum %d\n", select, buddy_tree[select].max_pgnum);
    buddy_tree[select].pgnum = buddy_tree[LCHILD(select)].pgnum + buddy_tree[RCHILD(select)].pgnum;
    buddy_tree[select].max_pgnum = MAX(buddy_tree[LCHILD(select)].max_pgnum, buddy_tree[RCHILD(select)].max_pgnum);
    Assert(select >= 0 && select <= leaf_base * 2, "select %d not in [0, %d]\n", select, leaf_base*2);
  }
}

static inline void update_buddy_free(int select){ // children of select are correct
  Assert(select >= 0 && select <= leaf_base * 2, "select %d not in [0, %d]\n", select, leaf_base*2);
  buddy_tree[select].type = buddy_tree[select].height == 0 ? BLOCK_FREE : BLOCK_INTERNAL;
  while(1){
    Assert((buddy_tree[select].max_pgnum & (next_pow2(buddy_tree[select].max_pgnum) - 1)) == 0, "invalid max_pgnum, select %d max_pgnum %d\n", select, buddy_tree[select].max_pgnum);
    Assert(select >= 0 && select <= leaf_base * 2, "select %d not in [0, %d]\n", select, leaf_base*2);
    buddy_tree[select].pgnum = buddy_tree[select].height == 0 ? 1 : buddy_tree[LCHILD(select)].pgnum + buddy_tree[RCHILD(select)].pgnum;
    int max_num = 1 << buddy_tree[select].height;
    buddy_tree[select].max_pgnum = buddy_tree[select].pgnum == max_num ? max_num :
                                  MAX(buddy_tree[LCHILD(select)].max_pgnum, buddy_tree[RCHILD(select)].max_pgnum);
    if(IS_ROOT(select)) break;
    select = PARENT(select);
  }
}

static void* global_newpage(){  // DFS & first fit
  spin_lock(&global_lock);

  int select=0;
  if(buddy_tree[select].pgnum == 0) {
    spin_unlock(&global_lock);
    return NULL;
  }
  for(int i = 0; i < buddy_tree[0].height; i++){
    Assert(buddy_tree[select].pgnum == buddy_tree[LCHILD(select)].pgnum + buddy_tree[RCHILD(select)].pgnum, "select %d select_pgnum %d lpgnum %d rpgnum %d", select, buddy_tree[select].pgnum, buddy_tree[LCHILD(select)].pgnum, buddy_tree[RCHILD(select)].pgnum);
    if(buddy_tree[LCHILD(select)].pgnum != 0) select = LCHILD(select);
    else select = RCHILD(select);
    Assert((select >= 0) && select <= leaf_base * 2, "select %d not in [0, %d]\n", select, leaf_base*2);
    Assert(buddy_tree[select].pgnum != 0, "select %d type %d height %d\n", select, buddy_tree[select].type, buddy_tree[select].height);
  }
  update_buddy_alloc(select);
  spin_unlock(&global_lock);
  int pgidx = select - leaf_base;
  void* ret = bstart + pgidx * PGSIZE;
  Assert(ret >= pheap_start && ret < pheap_end, "newpage 0x%lx not in [0x%lx, 0x%lx) bstart=0x%lx pgidx 0x%x\n", (uintptr_t)ret, (uintptr_t)pheap_start, (uintptr_t)pheap_end, bstart, pgidx);
  return ret;
}

static void* global_alloc(size_t size){
  int depth = sizeof(unsigned long)*8 - __builtin_clzl(size-1) - PG_BITS;
  if(depth < 0) depth = 0;
  int alloc_pgnum = 1 << depth;
  spin_lock(&global_lock);
  int select = 0;
  if(buddy_tree[select].max_pgnum < alloc_pgnum) {
    spin_unlock(&global_lock);
    return NULL;
  }
  for(int i = 0; i < (buddy_tree[0].height - depth); i++){
    Assert(select >= 0 && select <= leaf_base * 2, "select %d not in [0, %d]\n", select, leaf_base * 2);
    Assert(buddy_tree[select].pgnum == buddy_tree[LCHILD(select)].pgnum + buddy_tree[RCHILD(select)].pgnum, "select %d select_pgnum %d lpgnum %d rpgnum %d", select, buddy_tree[select].pgnum, buddy_tree[LCHILD(select)].pgnum, buddy_tree[RCHILD(select)].pgnum);
    if(buddy_tree[LCHILD(select)].max_pgnum >= alloc_pgnum) select = LCHILD(select);
    else select = RCHILD(select);
    Assert(buddy_tree[select].max_pgnum >= alloc_pgnum, "select %d type %d height %d max_pgnum\n", select, buddy_tree[select].type, buddy_tree[select].height, buddy_tree[select].max_pgnum);
  }
  update_buddy_alloc(select);
  int pgidx = select;
  while(buddy_tree[pgidx].height != 0) pgidx = LCHILD(pgidx);  // down to leaf
  spin_unlock(&global_lock);
  pgidx = pgidx - leaf_base;
  void* ret = bstart + pgidx * PGSIZE;
  Assert(ret >= pheap_start && ret < pheap_end, "newpage 0x%lx not in [0x%lx, 0x%lx) bstart=0x%lx pgidx %d\n", (uintptr_t)ret, (uintptr_t)pheap_start, (uintptr_t)pheap_end, bstart, pgidx);
  return ret;
}

static void global_free(void* ptr){
  Assert(((uintptr_t)ptr & PGMASK) == 0, "free addr 0x%lx\n", (uintptr_t)ptr);
  int pgidx = (uintptr_t)(ptr - bstart) / PGSIZE + leaf_base;
  spin_lock(&global_lock);
  while(pgidx != 0 && buddy_tree[pgidx].type != BLOCK_ALLOCATED) pgidx = PARENT(pgidx);
  Assert(buddy_tree[pgidx].type == BLOCK_ALLOCATED, "invalid free addr 0x%lx\n", (uintptr_t)ptr);
  update_buddy_free(pgidx);
  spin_unlock(&global_lock);
}

static smeta_t* slab_newpage(int bit, int cpu_id){
  Assert(bit >= 2 && bit <= 10, "bit_num %d is out of bound [2,10]\n", bit);
  smeta_t* ret = global_newpage();
  if(!ret) return NULL;
  ret->bit_num = bit;
  ret->magic = SLAB_MAGIC;
  ret->free_num = (PGSIZE - sizeof(smeta_t)) >> bit;
  ret->next = NULL;
  ret->cpu_id = cpu_id;
  memset(ret->bitmap, 0xff, sizeof(ret->bitmap));
  int bitmap_left = ret->free_num;
  int map_idx = 0;
  while(bitmap_left >= BITMAP_BITS){
    bitmap_left -= BITMAP_BITS;
    ret->bitmap[map_idx ++] = 0;
  }
  while(bitmap_left--){
    ret->bitmap[map_idx] = ret->bitmap[map_idx] << 1;
  }
  return ret;
}

static inline void set_bitmap(uint64_t* bitmap, int idx){
  Assert((bitmap[idx / BITMAP_BITS] & ((uint64_t)1 << (idx % BITMAP_BITS))) == 0, "bitmap is already set, idx %d bitmap %lx\n", idx, bitmap[idx / BITMAP_BITS]);
  bitmap[idx / BITMAP_BITS] |= (uint64_t)1 << (idx % BITMAP_BITS);
}

static inline void clear_bitmap(uint64_t* bitmap, int idx){
  Assert(bitmap[idx / BITMAP_BITS] & ((uint64_t)1 << (idx % BITMAP_BITS)), "bitmap is already cleared, idx %d bitmap %lx\n", idx, bitmap[idx / BITMAP_BITS]);
  bitmap[idx / BITMAP_BITS] &= ~((uint64_t)1 << (idx % BITMAP_BITS));
}

static void *kalloc(size_t size) {
  if(size > 1024) return global_alloc(size);
  int cpu_id = cpu_current();
  int bits = MAX(2, sizeof(unsigned long)*8 - __builtin_clzl(size-1));
  size = 1 << bits;
  int slab_idx = bits - 2;
  spin_lock(&(cpu_lock[cpu_id]));
  slab_t* select_slab = &slab[cpu_id][slab_idx];
  smeta_t* select_page = select_slab->page;
  Assert((size & ((1 << select_page->bit_num)-1)) == 0, "alloc size 0x%lx bit_num %d slab_idx %d", size, select_page->bit_num, slab_idx);
  Assert(select_page->bit_num == bits, "alloc select bit_num %d expected %d size 0x%lx slab_idx %d page 0x%lx", select_page->bit_num, bits, size, slab_idx, (uintptr_t)select_page);

  if(select_slab->total_num == 0){
    smeta_t* newpage = slab_newpage(bits, cpu_id);
    if(!newpage){
      spin_unlock(&cpu_lock[cpu_id]);
      return NULL;
    }
    newpage->next = select_page;
    select_slab->page = newpage;
    select_slab->total_num = newpage->free_num;
    select_page = newpage;
  }
  else{
    while(select_page->free_num == 0){
      select_page = select_page->next;
      Assert(select_page, "alloc size 0x%lx cpu_id %d slab_idx %d\n", size, cpu_id, slab_idx);
      Assert((size & ((1 << select_page->bit_num)-1)) == 0, "alloc size 0x%lx page 0x%lx bit_num %d slab_idx %d", size, select_page, select_page->bit_num, slab_idx);
    }
  }
  Assert(select_page->free_num > 0, "no free num, size 0x%lx page 0x%lx free_num %d", size, (uintptr_t)select_page, select_page->free_num);
  int map_idx = -1;
  for(int i = 0; i < (((PGSIZE - sizeof(smeta_t)) >> select_page->bit_num) + BITMAP_BITS - 1)/ BITMAP_BITS; i++){
    if(!BITMAP_FULL(select_page->bitmap[i])){
      map_idx = __builtin_ctzl(~select_page->bitmap[i]) + i * BITMAP_BITS;
      break;
    }
  }
  Assert(map_idx != -1, "invalid map_idx, size 0x%lx bits %d page 0x%lx free_num %d\n", size, select_page->bit_num, (uintptr_t)select_page, select_page->free_num);

  void* ret = (void*)select_page + PGSIZE - (map_idx + 1) * (1 <<select_page->bit_num);
  set_bitmap(select_page->bitmap, map_idx);
  select_slab->total_num --;
  select_page->free_num --;

  spin_unlock(&cpu_lock[cpu_id]);

  Assert(((uintptr_t)ret &(size-1)) == 0, "alloc size 0x%lx at 0x%lx page 0x%lx", size, (uintptr_t)ret, select_page);
  Assert(ROUNDDOWN((uintptr_t)ret, PGSIZE) == ROUNDDOWN((uintptr_t)ret + size - 1, PGSIZE), "alloc cross page, ret 0x%lx size 0x%lx\n", (uintptr_t)ret, size);
  Assert(((uintptr_t)ret & PGMASK) != 0, "ret can not be the beginning of page, alloc size 0x%lx at 0x%lx page 0x%lx free_num %d map_idx %d", size, (uintptr_t)ret, select_page, select_page->free_num, map_idx);
  Assert((uintptr_t)ret >= ROUNDDOWN((uintptr_t)ret, PGSIZE) + sizeof(smeta_t), "ret and metadata overlap, ret 0x%lx meta_size 0x%0x\n", (uintptr_t)ret, sizeof(smeta_t));
  return ret;
}

static void kfree(void *ptr) {
  if(!ptr) return;
  if(((uintptr_t)ptr & PGMASK) == 0) return global_free(ptr);
  smeta_t* select_page = (smeta_t*)ROUNDDOWN(ptr, PGSIZE);
  int cpu_id = select_page->cpu_id;
  spin_lock(&cpu_lock[cpu_id]);
  select_page->free_num ++;
  slab[cpu_id][select_page->bit_num-2].total_num ++;
  int bit_idx = (ROUNDUP(ptr, PGSIZE) - (uintptr_t)ptr) / (1 << select_page->bit_num) - 1;
  clear_bitmap(select_page->bitmap, bit_idx);
  spin_unlock(&cpu_lock[cpu_id]);
}

static void pmm_init() {
  pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

  pheap_start = (void*)ROUNDUP((uintptr_t)heap.start, PGSIZE);
  pheap_end = (void*)ROUNDDOWN((uintptr_t)heap.end, PGSIZE);
  memset(&global_lock, 0, sizeof(global_lock));
  strcpy(global_lock.name, "global");
  memset(&cpu_lock, 0, sizeof(cpu_lock));
  for(int i = 0; i < MAX_CPU; i++){
    sprintf(cpu_lock[i].name, "local%d", i);
  }
  /* init buddy */
  uintptr_t bsize = (uintptr_t)(pheap_end - pheap_start);
  int bheight = sizeof(unsigned long)*8 - __builtin_clzl(bsize-1) - PG_BITS;
  bsize = 1 << (bheight + PG_BITS);  // pmsize is not 1
  bstart = (void*)((uintptr_t)pheap_end - bsize);
  /* build buddy tree */
  size_t total_pgnum = bsize / PGSIZE;
  size_t invalid_pgnum = (bsize - (uintptr_t)(pheap_end - pheap_start)) / PGSIZE;
  size_t buddy_pgnum = ((2 * total_pgnum - 1) * sizeof(bentry_t) + PGSIZE-1) / PGSIZE;   // number of pages to store buddy tree. total_pgnum: leaf num
  buddy_tree = pheap_start;
  /* init leaves */
  int left = TREE_LAYER_BEGIN(bheight), right = TREE_LAYER_END(bheight);
  leaf_base = left;
  for(int i = left; i <= right; i++){
    bentry_t* cur_entry = buddy_tree + i;
    cur_entry->pgnum = 0;
    cur_entry->height = 0;
    cur_entry->max_pgnum = 0;

    int leaf_idx = i - left;
    /* [0, invalid): dummy entry; [invlaid, invalid+buddy) buddy tree pages */
    if(leaf_idx < invalid_pgnum) cur_entry->type = BLOCK_INVALID;
    else if(leaf_idx >= invalid_pgnum && leaf_idx < (invalid_pgnum + buddy_pgnum)) cur_entry->type = BLOCK_HEAD;
    else{
      cur_entry->type = BLOCK_FREE;
      cur_entry->pgnum = 1;
      cur_entry->max_pgnum = 1;
    }
  }
  /* init internal nodes */
  left = TREE_LAYER_BEGIN(0), right = TREE_LAYER_END(bheight-1);
  for(int i = right; i >= left; i--){
    buddy_tree[i].pgnum = buddy_tree[LCHILD(i)].pgnum + buddy_tree[RCHILD(i)].pgnum;
    buddy_tree[i].height = buddy_tree[LCHILD(i)].height + 1;
    buddy_tree[i].max_pgnum = buddy_tree[i].pgnum == (1 << buddy_tree[i].height) ? buddy_tree[i].pgnum : MAX(buddy_tree[LCHILD(i)].max_pgnum, buddy_tree[RCHILD(i)].max_pgnum);
    buddy_tree[i].type = BLOCK_INTERNAL;
    Assert(buddy_tree[LCHILD(i)].height == buddy_tree[RCHILD(i)].height, "idx %d lheight %d rheight %d\n", i, buddy_tree[LCHILD(i)].height, buddy_tree[RCHILD(i)].height);
  }
  Assert(buddy_tree[0].height == bheight, "root height=%d expect %d\n", buddy_tree[0].height, bheight);
  // init slab
  for(int i = 0; i < MAX_CPU; i++){
    for(int j = 0; j < SLAB_NUM; j++){
      slab[i][j].page = slab_newpage(j + 2, i);
      slab[i][j].total_num = slab[i][j].page->free_num;
    }
  }
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};

#ifdef PMM_DEBUG

void check_alloc(void* ptr, size_t size){
  uint8_t* p = (uint8_t*)ptr;
  for(int i = 0; i < size; i++){
    if(p[i] == ALLOC_MAGIC){
      Assert("double allocated at 0x%lx with size 0x%lx\n", ptr, size);
    }
  }
}

typedef struct pmm_workload{
  union{
    int pr[13];
    int accumulate[13];
  };
  int sum;
}pmm_workload;

pmm_workload          /*2, 4   8   16  32  64  128 256 512 1024 2048 4096 inf*/
  wl_typical = {.pr = {10, 10, 10, 40, 50, 40, 30, 20, 10, 4,   2,   1,   1}, .sum = 0 },
  wl_stress  = {.pr = {1,  0,  0,  400,200,100,1,  1,  1,  1,   1,   1,   1}, .sum = 0 },
  wl_page    = {.pr = {10, 0,  0,  1,  1,  1,  1,  1,  1,  1,   1,   1,   1}, .sum = 0 };

void* alloc_log[MAX_LOG_SIZE];
size_t size_log[MAX_LOG_SIZE];
size_t total_size;
int alloc_idx = 0;
int free_idx = 0;

lock_t log_lock;
lock_t util_lock;
static long long util_time;

void disp_util(){
  if(_sys_time() - util_time >= 10000000){
    spin_lock(&util_lock);
    printf("perc %d total %d MB used %d MB\n", (total_size*100) / pmsize, pmsize >> 20, total_size >> 20);
    util_time = _sys_time();
    spin_unlock(&util_lock);
  }
}

void pmm_workload_init(pmm_workload* wl){
  for(int i = 0; i < 12; i++){
    wl->sum += wl->pr[i];
    if(i != 0) wl->accumulate[i] += wl->accumulate[i-1];
  }
  memset(alloc_log, 0, sizeof(alloc_log));
  memset(size_log, 0, sizeof(size_log));
  total_size = 0;
  alloc_idx = 0;
  free_idx = 0;
  strcpy(util_lock.name, "util lock");
  util_lock.locked = 0;
  strcpy(log_lock.name, "log lock");
  log_lock.locked = 0;
  util_time = 0;
}

void pmm_debug_alloc(pmm_workload* wl){
  int num = rand() % wl->sum;
  int idx = 0;
  for(idx = 0; idx < 12; idx ++){
    if(num < wl->pr[idx]) break;
  }
  size_t size = 0, lbound = 1 << idx, ubound = 1 << (idx + 1);
  if(idx == 12) {       //(0.5pg-16pg]
    ubound = 16 * PGSIZE-1;
  }
  while(size < lbound || size >= ubound){
    size = rand() % ubound;
  }
  // printf("alloc size 0x%lx ", size);
  void* ptr = pmm->alloc(size);
  check_alloc(ptr, size);
  // printf("at 0x%lx alloc_idx %d \n", (uintptr_t)ptr, alloc_idx);
  spin_lock(&log_lock);
  if(ptr){
    alloc_log[alloc_idx] = ptr;
    size_log[alloc_idx] = size;
    total_size += size;
    alloc_idx = (alloc_idx + 1) & (MAX_LOG_SIZE - 1);
    disp_util();
  }
  spin_unlock(&log_lock);
  if(ptr) memset(ptr, ALLOC_MAGIC, size);
}

void pmm_debug_free(pmm_workload* wl){
  spin_lock(&log_lock);
  void* ptr = alloc_log[free_idx];
  if(ptr) {
    // printf("free %lx free_idx %d \n", (uintptr_t)ptr, free_idx);
    alloc_log[free_idx] = 0;
    total_size -= size_log[free_idx];
    free_idx = (free_idx + 1) & (MAX_LOG_SIZE - 1);
  }
  spin_unlock(&log_lock);
  pmm->free(ptr);
}

void pmm_test(){
  pmm_workload* workload = &wl_typical;
  pmm_workload_init(workload);

  while(1){
    int is_alloc = rand() % 3;
    if(is_alloc) pmm_debug_alloc(workload);
    else pmm_debug_free(workload);
  }
}

#endif

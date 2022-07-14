#include <common.h>
#include <kmt.h>
#include <uproc.h>
#include <elf.h>
#include <user.h>

void fill_standard_fd(task_t* task);

void *pgalloc(int size) {
  Assert((size % PGSIZE) == 0, "pgalloc: invalid size %ld\n", size);
  void* ret = pmm->alloc(size);
  return ret;
}

void pgfree(void *ptr) {
  pmm->free(ptr);
}

static inline int mmap_contains(mm_area_t* mm, void* addr){
  if(!mm) return 0;
  void* start = (void*)ROUNDDOWN(mm->start, PGSIZE);
  void* end = (void*)ROUNDUP(mm->end, PGSIZE);
  return (addr >= start ) && (addr < end);
}

Context* handle_pagefault(Event ev, Context* ctx){
  task_t* cur_task = kmt->gettask();
  void* pg_addr = (void*)ROUNDDOWN(ev.ref, PGSIZE);
  for(int i = 0; i < MAX_MMAP_NUM; i++){
    mm_area_t* mm = cur_task->mmaps[i];
    if(mmap_contains(mm, pg_addr)){
      void* pa = pgalloc(PGSIZE);
      if(!(mm->flags & MAP_ANONYMOUS)){
        Assert(cur_task->ofiles[mm->fd], "handle_pagefault: fd %d is not open\n", mm->fd);
        size_t start_offset = mm->offset + MAX(0, (uintptr_t)pg_addr - mm->start);
        uintptr_t start_pa = start_offset == mm->offset ? mm->start - (uintptr_t)pg_addr : 0;
        uintptr_t end_pa = (uintptr_t)pg_addr == ROUNDDOWN(mm->end, PGSIZE) ? mm->end - (uintptr_t)pg_addr : PGSIZE;
        Assert(start_pa >= 0 && start_pa < PGSIZE && end_pa >= 0 && end_pa <= PGSIZE, "handle_pagefault: start 0x%lx end 0x%lx\n", start_pa, end_pa);
        vfs->lseek(mm->fd, start_offset, SEEK_SET);
        vfs->read(mm->fd, pa + start_pa, end_pa - start_pa);
        asm volatile ("fence.i");
      }
      map(cur_task->as, pg_addr, pa, mm->prot);
      return NULL;
    }
  }
  Assert(0, "pagefault: invalid addr 0x%lx pc=0x%lx\n", ev.ref, ctx->epc);
}


Context* do_syscall(Event ev, Context* context);
static void uproc_init(){
  os->on_irq(0, EVENT_SYSCALL, do_syscall);
  os->on_irq(0, EVENT_PAGEFAULT, handle_pagefault);
}

static int uproc_mmap(void *addr, size_t len, int prot, int flags, int fd, size_t offset){
  task_t* task = kmt->gettask();
  for(int i = 0; i < MAX_MMAP_NUM; i++){
    if(!task->mmaps[i]){
      mm_area_t* tmp_mm = pmm->alloc(sizeof(mm_area_t));
      task->mmaps[i] = tmp_mm;
      tmp_mm->start = (uintptr_t)addr;
      tmp_mm->end = (uintptr_t)addr + len;
      tmp_mm->fd = fd;
      tmp_mm->prot = prot;
      tmp_mm->flags = flags;
      tmp_mm->offset = offset;
      return 0;
    }
  }
  printf("mmaps full\n");
  return -1;
}

static int uproc_fork(){
  task_t* cur_task = kmt->gettask();
  task_t* new_task = pmm->alloc(sizeof(task_t));
  kmt_initforktask(new_task, cur_task->name);
  // copy context
  memcpy(TOP_CONTEXT(new_task), TOP_CONTEXT(cur_task), sizeof(Context));
  // child share the same ofile with parent
  fill_standard_fd(new_task);
  for(int i = STDERR_FILENO + 1; i < MAX_OPEN_FILE; i++){
    if(cur_task->ofiles[i]){
      new_task->ofiles[i] = filedup(cur_task->ofiles[i]);
    }
  }
  for(int i = 0; i < MAX_MMAP_NUM; i++){
    if(cur_task->mmaps[i]){
      new_task->mmaps[i] = pmm->alloc(sizeof(mm_area_t));
      memcpy(new_task->mmaps[i], cur_task->mmaps[i], sizeof(mm_area_t));
    }
  }

  new_task->kstack = pmm->alloc(STACK_SIZE);
  memcpy(new_task->kstack, cur_task->kstack, STACK_SIZE);
  new_task->cwd = dup_dirent(cur_task->cwd);
  new_task->cwd_type = cur_task->cwd_type;
  new_task->max_brk = cur_task->max_brk;
  // copy pagetable
  AddrSpace* as = pmm->alloc(sizeof(AddrSpace));
  protect(as);
  new_task->as = as;
  pgtable_ucopy(cur_task->as->ptr, new_task->as->ptr);
  // fork return 0 in child
  TOP_CONTEXT(new_task)->gpr[NO_A0] = 0;
  TOP_CONTEXT(new_task)->satp = MAKE_SATP(as->ptr);
  TOP_CONTEXT(new_task)->kernel_sp = TOP_CONTEXT(cur_task)->kernel_sp - (uintptr_t)cur_task->kstack + (uintptr_t)new_task->kstack;
  kmt_inserttask(new_task, 1);
  return new_task->pid;
}

static int uproc_execve(const char *path, char *argv[], char *envp[]){
  // release resources
  task_t* task = kmt->gettask();
  execve_release_resources(task);
  AddrSpace* oldas = task->as;
  AddrSpace* as = pmm->alloc(sizeof(AddrSpace));
  protect(as);
  if(task->stack == task->kstack) task->stack = pmm->alloc(STACK_SIZE);
  for(int i = 0; i < STACK_SIZE / PGSIZE; i++){
    map(as, as->area.end - STACK_SIZE + i * PGSIZE, task->stack + i * PGSIZE, PROT_READ|PROT_WRITE);
  }

  uintptr_t* args_ptr = (uintptr_t*)(STACK_END(task->stack)) - 1;
#if 0
  TODO(); // argv in user space
  if(argv){
    int argc = 0;
    for(argc = 0; argv[argc]; argc ++) printf("0x%lx, 0x%lx\n", argv, argv[argc]); // count the number of arg
    *args_ptr = argc;
    uintptr_t str_start = (uintptr_t)STACK_END(task->stack) - (argc + 2) * sizeof(uintptr_t);  // argc; args; NULL;
    int offset = sizeof(uintptr_t);
    for(int i = 0; i < argc; i++){
      *(args_ptr-i-1) = as->area.end - STACK_SIZE + str_start - task->stack;
      int str_len = strlen(argv[i]);
      strcpy((void*)str_start, argv[i]);
      str_start += str_len + 1; // string follewed by 0
      offset += sizeof(uintptr_t);
    }
    *(uintptr_t*)(STACK_START(task->stack) + offset) = 0;
  }
#else
  *args_ptr = 0;
#endif
  task->int_depth = 1;
  SET_TASK(task);

  fill_standard_fd(task);

  int fd = vfs->openat(AT_FDCWD, path, O_RDONLY);
  task->cwd = kmt->gettask()->ofiles[fd]->dirent->parent;
  if(fd < 0){
    printf("execve: open %s fail\n", path);
    return -1;
  }

  Elf64_Ehdr _Eheader;
  vfs->read(fd, &_Eheader, sizeof(_Eheader));
  if(*(uint32_t *)(&_Eheader.e_ident) != 0x464c457f){
    printf("execve: %s is not a elf file\n", path);
    return -1;
  }

  for(int i = 0; i < _Eheader.e_phnum; i++){
    Elf64_Phdr _Pheader;
    vfs->lseek(fd,  _Eheader.e_phoff + i * _Eheader.e_phentsize, SEEK_SET);
    int rdsize = vfs->read(fd, &_Pheader, sizeof(_Pheader));
    Assert(rdsize == sizeof(_Pheader), "execve: rdsize 0x%x, expect 0x%x\n", rdsize, sizeof(_Pheader));
    if(_Pheader.p_type == PT_LOAD){
      uproc_mmap((void*)_Pheader.p_vaddr, _Pheader.p_filesz, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, _Pheader.p_offset);
      uintptr_t filesz_end = ROUNDUP(_Pheader.p_filesz + _Pheader.p_vaddr, PGSIZE);
      if(filesz_end != ROUNDUP(_Pheader.p_vaddr + _Pheader.p_memsz, PGSIZE)){
        uproc_mmap((void*)filesz_end, _Pheader.p_memsz + _Pheader.p_vaddr - filesz_end, PROT_READ| PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      }
      task->max_brk = (void*)ROUNDUP(_Pheader.p_vaddr + _Pheader.p_memsz, PGSIZE);
      task->brk = task->max_brk;
    }
  }
// #endif
  TOP_CONTEXT(task) = ucontext(as, (Area){.start = (void*)STACK_START(task->kstack), .end = (void*)STACK_END(task->kstack)}, (void*)_Eheader.e_entry);
  task->blocked = 0;
  task->as = as;
  task->name = path;

  TOP_CONTEXT(task)->gpr[NO_A0] = (uintptr_t)(as->area.end - STACK_SIZE + STACK_START(task->stack) - task->stack);
  TOP_CONTEXT(task)->gpr[NO_SP] -= 8;

  free_pages(oldas);
  return 0;
}

static int uproc_exit(){
  task_t* cur_task = kmt->gettask();
  RUN_STATE(cur_task) = TASK_DEAD;
  return 0;
}

static int uproc_brk(void* addr){
  task_t* cur_task = kmt->gettask();
  if(cur_task->max_brk >= addr) {
    cur_task->brk = MAX(cur_task->brk, addr);
    return cur_task->brk;
  }
  while(cur_task->max_brk < addr){
    void* pa = pgalloc(PGSIZE);
    map(cur_task->as, cur_task->max_brk, pa, PROT_READ|PROT_WRITE);
    cur_task->max_brk += PGSIZE;
  }
  cur_task->brk = addr;
  return cur_task->brk;
}

char* programs[] = {"open", "close", "execve", "getpid", "read", "write", "brk", /*"chdir",*/ \
                    "fstat", /*"mkdir_",*/ "getcwd", "yield", "exit", "times", "unlink", "dup", \
                    "mmap", "munmap", "pipe", "umount", "wait", "clone","mount", "sleep", \
                    "dup2", "fork", "getdents", "gettimeofday", "uname", "waitpid"};

spinlock_t id_lock;
static volatile int id = 0;
#include <syscall.h>

void next_id(){
  kmt->spin_lock(&id_lock);
  id ++;
  kmt->spin_unlock(&id_lock);
}

#ifdef UPROC_DEBUG
void exec_program(){
  char* path = "/open";
  int do_syscall3(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3);
  char* args[] = {
    0
  };

  do_syscall3(SYS_execve, (uintptr_t)programs[id], (uintptr_t)args, 0);

  Assert(0, "should not reach here\n");
}

void uproc_test(void* args){
  while(id < ((uint8_t*)args)[0]);
  w_csr("sepc", (uintptr_t)exec_program);
  asm volatile("sret");
  Assert(0, "should not reach here\n");
}
#endif

extern char  _initcode_start, _initcode_end;

void start_initcode(void* args){
  task_t* cur_task = kmt->gettask();
  for(int i = 0; i < (uintptr_t)(&_initcode_end - &_initcode_start); i += PGSIZE){
    printf("map i=0x%lx [0x%lx, 0x%lx) \n", i, &_initcode_start, &_initcode_end);
    map(cur_task->as, (void*)i, &_initcode_start + i, 0);
  }
  printf("start initcode...\n");
  w_csr("sepc", 0);
  asm volatile("sret");
  Assert(0, "should not reach here\n");
}


MODULE_DEF(uproc) = {
	.init   = uproc_init,
	.mmap   = uproc_mmap,
	.fork   = uproc_fork,
	.execve = uproc_execve,
  .brk    = uproc_brk,
	.exit   = uproc_exit
};


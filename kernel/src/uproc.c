#include <common.h>
#include <kmt.h>
#include <uproc.h>
#include <elf.h>
#include <sys/mman.h>
#include <user.h>

void *pgalloc(int size) {
  Assert((size % PGSIZE) == 0, "pgalloc: invalid size %ld\n", size);
  void* ret = pmm->alloc(size);
  memset(ret, 0, size);
  return ret;
}

void pgfree(void *ptr) {
  pmm->free(ptr);
}

static inline int mmap_contains(mm_area_t* mm, void* addr){
  if(!mm) return 0;
  void* start = (void*)ROUNDDOWN(mm->start, PGSIZE);
  void* end = (void*)ROUNDUP(mm->end, PGSIZE);
  printf("check 0x%lx [0x%lx, 0x%lx)\n", (uintptr_t)addr, mm->start, mm->end);
  return (addr >= start ) && (addr < end);
}

Context* handle_pagefault(Event ev, Context* ctx){
  printf("pgfault addr 0x%lx\n", ev.ref);
  task_t* cur_task = kmt->gettask();
  void* pg_addr = (void*)ROUNDDOWN(ev.ref, PGSIZE);
  for(int i = 0; i < MAX_MMAP_NUM; i++){
    mm_area_t* mm = cur_task->mmaps[i];
    if(mmap_contains(mm, pg_addr)){
      void* pa = pgalloc(PGSIZE);
      if(!(mm->prot & MAP_ANONYMOUS)){
        Assert(cur_task->ofiles[mm->fd], "handle_pagefault: fd %d is not open\n", mm->fd);
        off_t start_offset = mm->offset + MAX(0, (uintptr_t)pg_addr - mm->start); // 
        uintptr_t start_pa = start_offset == mm->offset ? mm->start - (uintptr_t)pg_addr : 0;
        uintptr_t end_pa = (uintptr_t)pg_addr == ROUNDDOWN(mm->end, PGSIZE) ? mm->end - (uintptr_t)pg_addr : PGSIZE;
        Assert(start_pa >= 0 && start_pa < PGSIZE && end_pa >= 0 && end_pa < PGSIZE, "handle_pagefault: start %x end %x\n", start_pa, end_pa);
        vfs->lseek(mm->fd, mm->offset, SEEK_SET);
        vfs->read(mm->fd, pa + start_pa, end_pa - start_pa);
      }
      printf("map va 0x%lx pa 0x%lx\n", pg_addr, pa);
      map(cur_task->as, pg_addr, pa, mm->prot);
      return NULL;
    }
  }
  Assert(0, "pagefault: invalid addr %lx\n", ev.ref);
}


Context* do_syscall(Event ev, Context* context);
static void uproc_init(){
  os->on_irq(0, EVENT_SYSCALL, do_syscall);
  os->on_irq(0, EVENT_PAGEFAULT, handle_pagefault);
}

static int uproc_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset){
  task_t* task = kmt->gettask();
  printf("mmap addr 0x%lx len 0x%lx offset 0x%lx\n", addr, len, offset);
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
  kmt_newforktask(new_task, cur_task->name);
  // copy context
  memcpy(TOP_CONTEXT(new_task), TOP_CONTEXT(cur_task), sizeof(Context));
  // fork return 0 in child
  TOP_CONTEXT(new_task)->rax = 0;
  // child share the same ofile with parent
  for(int i = 0; i < MAX_OPEN_FILE; i++){
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
  new_task->cwd_inode_no = cur_task->cwd_inode_no;
  new_task->cwd_type = cur_task->cwd_type;
  // TODO: page rdonly
  return 0; // TODO: return pid
}

static int uproc_execve(const char *path, char *argv[], char *envp[]){
  // release resources
  task_t* task = kmt->gettask();
  printf("execve task 0x%lx\n", task);
  release_resources_except_stack(task);
  AddrSpace* as = pmm->alloc(sizeof(AddrSpace));
  protect(as);
  task->stack = pmm->alloc(STACK_SIZE);
  for(int i = 0; i < STACK_SIZE / PGSIZE; i++){
    map(as, as->area.end - STACK_SIZE + i * PGSIZE, task->stack + i * PGSIZE, PROT_READ|PROT_WRITE);
  }
  task->int_depth = 1;
  SET_TASK(task);
  void fill_standard_fd(task_t* task);
  fill_standard_fd(task);

  int fd = vfs->open(path, O_RDONLY);
  if(fd < 0){
    printf("execve: open %s fail\n");
    return -1;
  }
// #if defined(__ISA_X86_64__)
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
    }
  }
// #endif
  printf("entry 0x%lx\n", _Eheader.e_entry);
  TOP_CONTEXT(task) = ucontext(as, (Area){.start = (void*)STACK_START(task->kstack), .end = (void*)STACK_END(task->kstack)}, (void*)_Eheader.e_entry);
  task->blocked = 0;
  task->as = as;

  // TODO: /proc update

  return 0;
}

static int uproc_exit(){
  task_t* cur_task = kmt->gettask();
  kmt->teardown(cur_task);
  clear_current_task();
  return 0;
}

#include <syscall.h>
void hello_test(){
  char* path = "/hello";
  void do_syscall3(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3);
  do_syscall3(SYS_EXECVE, (uintptr_t)path, 0, 0);
  // uproc_execve(path, NULL, NULL);
  Assert(0, "should not reach here\n");
}

MODULE_DEF(uproc) = {
	.init   = uproc_init,
	.mmap   = uproc_mmap,
	.fork   = uproc_fork,
	.execve = uproc_execve,
	.exit   = uproc_exit
};


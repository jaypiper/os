#include <common.h>
#include <syscall.h>
#include <vfs.h>
#include <kmt.h>

/*  syscall #: rax
    return val: rax
    args: rdi   rsi   rdx   r10   r8    r9
*/
#define BUF_LEN 64
static uint8_t buf[6][BUF_LEN];

enum{ARG_NUM = 1, ARG_BUF, ARG_PTR};
// TODO: add function for every arg type

static inline void copy_from_user(Context* ctx, void* dst, uintptr_t user_addr, size_t count){
  size_t copied_size = 0;
  while(copied_size < count){
    uintptr_t start_size = user_addr + copied_size;
    uintptr_t pg_offset = start_size & (PGSIZE - 1);
    uintptr_t pa = user_addr_translate(ctx->satp, start_size - pg_offset) + pg_offset;
    size_t cpy_size = MIN(ROUNDUP(start_size + 1, PGSIZE) - start_size, count - copied_size);
    memcpy(dst + copied_size, (void*)pa, cpy_size);
    copied_size += cpy_size;

  }
}

static inline uintptr_t argraw(int n, Context* ctx, int type){
  Assert(0 <= n && n <= 5, "argraw: invalid n %d\n", n);
  uintptr_t status;
  r_csr("sstatus", status);

  if((status & SSTATUS_SPP) == 0){
    if(type == ARG_BUF){
      copy_from_user(ctx, buf[n], ctx->gpr[NO_A0 + n], BUF_LEN);
      return (uintptr_t)buf[n];
    } else if(type == ARG_PTR){
      uintptr_t pg_offset = ctx->gpr[NO_A0 + n] & (PGSIZE - 1);
      return user_addr_translate(ctx->satp, ctx->gpr[NO_A0 + n] - pg_offset) + pg_offset;
    }
  }
  return ctx->gpr[NO_A0 + n];  // a0-a5
}

int sys_chdir(Context* ctx){ // const char *path
  uintptr_t path = argraw(0, ctx, ARG_BUF);
  return vfs->chdir((char*)path);
}

int sys_close(Context* ctx){ // int fd
  int fd = argraw(0, ctx, ARG_NUM);
  return vfs->close(fd);
}

int sys_dup(Context* ctx){ // int oldfd
  int fd = argraw(0, ctx, ARG_NUM);
  return vfs->dup(fd);
}

int sys_execve(Context* ctx){ // const char *pathname, char *const argv[], char *const envp[]
  uintptr_t pathname = argraw(0, ctx, ARG_BUF);
  uintptr_t argv = argraw(1, ctx, ARG_BUF);
  uintptr_t envp = 0; //argraw(2, ctx, ARG_BUF);
  return uproc->execve((char*)pathname, (char**)argv, (char**)envp);
}

int sys_exit(Context* ctx){ // int status
  void next_id();
  next_id();
  return uproc->exit();
}

int sys_fork(Context* ctx){
  return uproc->fork();
}

int sys_fstat(Context* ctx){ // int fd, struct stat *statbuf
  int fd = argraw(0, ctx, ARG_NUM);
  uintptr_t statbuf = argraw(1, ctx, ARG_PTR);
  return vfs->fstat(fd, (void*)statbuf);
}

int sys_link(Context* ctx){ // const char *oldpath, const char *newpath
  uintptr_t oldpath = argraw(0, ctx, ARG_BUF);
  uintptr_t newpath = argraw(1, ctx, ARG_BUF);
  return vfs->link((char*)oldpath, (char*)newpath);
}

int sys_lseek(Context* ctx){ // int fd, size_t offset, int whence
  int fd = argraw(0, ctx, ARG_NUM);
  int offset = argraw(1, ctx, ARG_NUM);
  int whence = argraw(2, ctx, ARG_NUM);
  return vfs->lseek(fd, offset, whence);
}

int sys_mkdirat(Context* ctx){ // int dirfd, const char *pathname, mode_t mode
int dirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t pathname = argraw(1, ctx, ARG_BUF);
  return vfs->mkdirat(dirfd, (char*)pathname);
}

int sys_mmap(Context* ctx){ // void *addr, size_t length, int prot, int flags, int fd, size_t offset
  uintptr_t addr = argraw(0, ctx, ARG_NUM);
  uintptr_t size = argraw(1, ctx, ARG_NUM);
  int prot = argraw(2, ctx, ARG_NUM);
  int flags = argraw(3, ctx, ARG_NUM);
  int fd = argraw(4, ctx, ARG_NUM);
  uintptr_t offset = argraw(5, ctx, ARG_NUM);
  return uproc->mmap((char*)addr, size, prot, flags, fd, offset);
}

int sys_openat(Context* ctx){ // int dirfd, const char *pathname, int flags
  int dirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t pathname = argraw(1, ctx, ARG_BUF);
  int flags = argraw(2, ctx, ARG_NUM);
  return vfs->openat(dirfd, (char*)pathname, flags);
}

int sys_read(Context* ctx){ // int fd, void *buf, size_t count
  int fd = argraw(0, ctx, ARG_NUM);
  uintptr_t buf = argraw(1, ctx, ARG_PTR);
  uintptr_t count = argraw(2, ctx, ARG_NUM);
  return vfs->read(fd, (void*)buf, count);
}

int sys_unlink(Context* ctx){ // const char *pathname
  uintptr_t pathname = argraw(0, ctx, ARG_BUF);
  return vfs->unlink((char*)pathname);
}

int sys_write(Context* ctx){ // int fd, const void *buf, size_t count
  int fd = argraw(0, ctx, ARG_NUM);
  uintptr_t buf = argraw(1, ctx, ARG_BUF);
  uintptr_t count = argraw(2, ctx, ARG_NUM);
  return vfs->write(fd, (void*)buf, count);
}

int sys_brk(Context* ctx){ // void *addr
  uintptr_t addr = argraw(0, ctx, ARG_NUM);
  return uproc->brk((void*)addr);
}

int sys_getpid(Context* ctx){
  return kmt->gettask()->pid;
}

int sys_getcwd(Context* ctx){ // char *buf, size_t size
  uintptr_t buf = argraw(0, ctx, ARG_BUF);
  uintptr_t size = argraw(1, ctx, ARG_NUM);
  return vfs->getcwd(buf, size);
}

static int (*syscalls[MAX_SYSCALL_IDX])() = {
[SYS_chdir]     = sys_chdir,
[SYS_close]     = sys_close,
[SYS_dup]       = sys_dup,
[SYS_execve]    = sys_execve,
[SYS_exit]      = sys_exit,
// [SYS_fork]      = sys_fork,
[SYS_fstat]     = sys_fstat,
// [SYS_link]      = sys_link,
[SYS_lseek]     = sys_lseek,
[SYS_mkdirat]     = sys_mkdirat,
[SYS_mmap]      = sys_mmap,
[SYS_openat]      = sys_openat,
[SYS_read]      = sys_read,
// [SYS_unlink]    = sys_unlink,
[SYS_write]     = sys_write,
[SYS_brk]       = sys_brk,
[SYS_getpid]    = sys_getpid,
[SYS_getcwd]    = sys_getcwd,
};

Context* do_syscall(Event ev, Context* context){
  iset(true);
  int syscall_no = context->gpr[17];
  Assert(syscall_no < MAX_SYSCALL_IDX, "invalid syscall 0x%x\n", syscall_no);
  int (*sys_handler)() = syscalls[syscall_no];
  Assert(sys_handler, "invalid syscall 0x%x\n", syscall_no);
  int ret = sys_handler(context);
  TOP_CONTEXT(kmt->gettask())->gpr[NO_A0] = ret;
  return NULL;
}

int do_syscall3(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3){
  int ret;
  asm volatile("mv a0, %1; \
                mv a1, %2; \
                mv a2, %3; \
                mv a7, %4; \
                ecall; \
                mv a0, %0" : "=r"(ret) : "r"(val1), "r"(val2), "r"(val3), "r"((unsigned long long)syscall) : "%a0", "%a1", "%a2", "%a7");
  return ret;
}

void do_syscall2(int syscall, unsigned long long val1, unsigned long long val2){
  asm volatile("mv a0, %0; \
                mv a1, %1; \
                mv a7, %2; \
                ecall" : : "r"(val1), "r"(val2), "r"((unsigned long long)syscall));
}

void do_syscall1(int syscall, long long val1){
  asm volatile("mv a0, %0; \
                mv a7, %1; \
                ecall" : : "r"(val1), "r"((unsigned long long)syscall));
}

#include <common.h>
#include <syscall.h>
#include <vfs.h>

/*  syscall #: rax
    return val: rax
    args: rdi   rsi   rdx   r10   r8    r9
*/
static inline uintptr_t argraw(int n, Context* ctx){
  Assert(0 <= n && n <= 5, "argraw: invalid n %d\n", n);
  return ctx->gpr[10 + n];  // a0-a5
}

int sys_chdir(Context* ctx){ // const char *path
  uintptr_t path = argraw(0, ctx);
  return vfs->chdir((char*)path);
}

int sys_close(Context* ctx){ // int fd
  int fd = argraw(0, ctx);
  return vfs->close(fd);
}

int sys_dup(Context* ctx){ // int oldfd
  int fd = argraw(0, ctx);
  return vfs->dup(fd);
}

int sys_execve(Context* ctx){ // const char *pathname, char *const argv[], char *const envp[]
  uintptr_t pathname = argraw(0, ctx);
  uintptr_t argv = argraw(1, ctx);
  uintptr_t envp = argraw(2, ctx);
  return uproc->execve((char*)pathname, (char**)argv, (char**)envp);
}

int sys_exit(Context* ctx){ // int status
  return uproc->exit();
}

int sys_fork(Context* ctx){
  return uproc->fork();
}

int sys_fstat(Context* ctx){ // int fd, struct stat *statbuf
  int fd = argraw(0, ctx);
  uintptr_t statbuf = argraw(1, ctx);
  return vfs->fstat(fd, (void*)statbuf);
}

int sys_link(Context* ctx){ // const char *oldpath, const char *newpath
  uintptr_t oldpath = argraw(0, ctx);
  uintptr_t newpath = argraw(1, ctx);
  return vfs->link((char*)oldpath, (char*)newpath);
}

int sys_lseek(Context* ctx){ // int fd, off_t offset, int whence
  int fd = argraw(0, ctx);
  int offset = argraw(1, ctx);
  int whence = argraw(2, ctx);
  return vfs->lseek(fd, offset, whence);
}

int sys_mkdir(Context* ctx){ // const char *pathname, mode_t mode
  uintptr_t pathname = argraw(0, ctx);
  return vfs->mkdir((char*)pathname);
}

int sys_mmap(Context* ctx){ // void *addr, size_t length, int prot, int flags, int fd, off_t offset
  uintptr_t addr = argraw(0, ctx);
  uintptr_t size = argraw(1, ctx);
  int prot = argraw(2, ctx);
  int flags = argraw(3, ctx);
  int fd = argraw(4, ctx);
  uintptr_t offset = argraw(5, ctx);
  return uproc->mmap((char*)addr, size, prot, flags, fd, offset);
}

int sys_open(Context* ctx){ // const char *pathname, int flags
  uintptr_t pathname = argraw(0, ctx);
  int flags = argraw(1, ctx);
  return vfs->open((char*)pathname, flags);
}

int sys_read(Context* ctx){ // int fd, void *buf, size_t count
  int fd = argraw(0, ctx);
  uintptr_t buf = argraw(1, ctx);
  uintptr_t count = argraw(2, ctx);
  return vfs->read(fd, (void*)buf, count);
}

int sys_unlink(Context* ctx){ // const char *pathname
  uintptr_t pathname = argraw(0, ctx);
  return vfs->unlink((char*)pathname);
}

int sys_write(Context* ctx){ // int fd, const void *buf, size_t count
  int fd = argraw(0, ctx);
  uintptr_t buf = argraw(1, ctx);
  uintptr_t count = argraw(2, ctx);
  return vfs->write(fd, (void*)buf, count);
}

int sys_brk(Context* ctx){ // void *addr
  uintptr_t addr = argraw(0, ctx);
  return uproc->brk((void*)addr);
}


static int (*syscalls[MAX_SYSCALL_IDX])() = {
[SYS_CHDIR]     = sys_chdir,
[SYS_CLOSE]     = sys_close,
[SYS_DUP]       = sys_dup,
[SYS_EXECVE]    = sys_execve,
[SYS_EXIT]      = sys_exit,
[SYS_FORK]      = sys_fork,
[SYS_FSTAT]     = sys_fstat,
[SYS_LINK]      = sys_link,
[SYS_LSEEK]     = sys_lseek,
[SYS_MKDIR]     = sys_mkdir,
[SYS_MMAP]      = sys_mmap,
[SYS_OPEN]      = sys_open,
[SYS_READ]      = sys_read,
[SYS_UNLINK]    = sys_unlink,
[SYS_WRITE]     = sys_write,
[SYS_BRK]       = sys_brk,
};

Context* do_syscall(Event ev, Context* context){
  iset(true);
  int syscall_no = context->gpr[17];
  Assert(syscall_no < MAX_SYSCALL_IDX, "invalid syscall 0x%x\n", syscall_no);
  int (*sys_handler)() = syscalls[syscall_no];
  Assert(sys_handler, "invalid syscall 0x%x\n", syscall_no);
  int ret = sys_handler(context);
  context->gpr[0] = ret;
  return NULL;
}

void do_syscall3(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3){
  asm volatile("mv a0, %0; \
                mv a1, %1; \
                mv a2, %2; \
                mv a7, %3; \
                ecall" : : "r"(val1), "r"(val2), "r"(val3), "r"((unsigned long long)syscall) : "%a0", "%a1", "%a2", "%a7");
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

#include <common.h>
#include <syscall.h>
#include <sys_struct.h>
#include <vfs.h>
#include <kmt.h>

/*  syscall #: rax
    return val: rax
    args: rdi   rsi   rdx   r10   r8    r9
*/
#define BUF_LEN 64
#define STR_LEN 32
static uint8_t buf[6][BUF_LEN];
static char strs[6][STR_LEN];
enum{ARG_NUM = 1, ARG_BUF, ARG_PTR, ARG_ARGV};
// TODO: add function for every arg type

void copy_from_user(Context* ctx, void* dst, uintptr_t user_addr, size_t count){
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

void copy_to_user(Context* ctx, void* src, uintptr_t user_addr, size_t count){
  size_t copied_size = 0;
  while(copied_size < count){
    uintptr_t start_size = user_addr + copied_size;
    uintptr_t pg_offset = start_size & (PGSIZE - 1);
    uintptr_t pa = user_addr_translate(ctx->satp, start_size - pg_offset) + pg_offset;
    size_t cpy_size = MIN(ROUNDUP(start_size + 1, PGSIZE) - start_size, count - copied_size);
    memcpy((void*)pa, src + copied_size, cpy_size);
    copied_size += cpy_size;

  }
}

static inline uintptr_t argraw(int n, Context* ctx, int type){
  Assert(0 <= n && n <= 5, "argraw: invalid n %d\n", n);
  uintptr_t status;
  r_csr("sstatus", status);
  int count = BUF_LEN;
  int offset = 0, copied_size = 0;

  if((status & SSTATUS_SPP) == 0){
    if(type == ARG_BUF){
      while(count){
        copied_size = MIN(count, PGSIZE - ctx->gpr[NO_A0 + n] & 0xfff);
        copy_from_user(ctx, &buf[n][offset], ctx->gpr[NO_A0 + n], copied_size);
        for(int i = 0; i < copied_size; i++){
          if(buf[n][offset + i] == 0) break;
        }
        count -= copied_size;
        offset += copied_size;
      }
      return (uintptr_t)buf[n];
    } else if(type == ARG_PTR){
      uintptr_t pg_offset = ctx->gpr[NO_A0 + n] & (PGSIZE - 1);
      return user_addr_translate(ctx->satp, ctx->gpr[NO_A0 + n] - pg_offset) + pg_offset;
    } else if(type == ARG_ARGV){
      copy_from_user(ctx, buf[n], ctx->gpr[NO_A0 + n], BUF_LEN);
      uintptr_t** argv = (uintptr_t**)buf[n];
      int i = 0;
      for(i = 0; argv[i]; i ++){
        copy_from_user(ctx, strs[i], argv[i], STR_LEN);
        argv[i] = strs[i];
      }
      return (uintptr_t)buf[n];
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
  uintptr_t argv = argraw(1, ctx, ARG_ARGV);
  uintptr_t envp = 0; //argraw(2, ctx, ARG_BUF);
  return uproc->execve((char*)pathname, (char**)argv, (char**)envp);
}

int sys_exit(Context* ctx){ // int status
  void next_id();
  next_id();
  int status = argraw(0, ctx, ARG_NUM);
  return uproc->exit(status);
}

int sys_fstat(Context* ctx){ // int fd, struct stat *statbuf
  int fd = argraw(0, ctx, ARG_NUM);
  uintptr_t stataddr = argraw(1, ctx, ARG_NUM);
  stat buf;
  int ret = vfs->fstat(fd, &buf);
  copy_to_user(ctx, &buf, stataddr, sizeof(stat));
  return ret;
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

int sys_munmap(Context* ctx){
  uintptr_t addr = argraw(0, ctx, ARG_NUM);
  uintptr_t len = argraw(1, ctx, ARG_NUM);
  task_t* task = kmt->gettask();
  for(int i = 0; i < MAX_MMAP_NUM; i++){
    mm_area_t* mm_area = task->mmaps[i];
    if(mm_area && mm_area->start == addr && (mm_area->end - mm_area->start) == len){
      pmm->free(mm_area);
      task->mmaps[i] = NULL;
      break;
    }
  }
  return 0;
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

int sys_write(Context* ctx){ // int fd, const void *buf, size_t count
  int fd = argraw(0, ctx, ARG_NUM);
  uintptr_t addr = argraw(1, ctx, ARG_NUM);
  uintptr_t count = argraw(2, ctx, ARG_NUM);
  char buf[count + 1];
  copy_from_user(ctx, buf, addr, count);
  int ret = vfs->write(fd, (void*)buf, count);
  return ret;
}

int sys_brk(Context* ctx){ // void *addr
  uintptr_t addr = argraw(0, ctx, ARG_NUM);
  return uproc->brk((void*)addr);
}

int sys_gettid(Context* ctx){
  return kmt->gettask()->pid;
}

int sys_getpid(Context* ctx){
  return kmt->gettask()->tgid;
}

int sys_getcwd(Context* ctx){ // char *buf, size_t size
  uintptr_t buf = argraw(0, ctx, ARG_NUM);
  uintptr_t size = argraw(1, ctx, ARG_NUM);
  char path[64];
  memset(path, 0, sizeof(path));
  vfs->getcwd(path, size);
  copy_to_user(ctx, path, buf, strlen(path));
  return buf;
}

/*  check user's permissions of a file relative to a directory file descriptor*/
int sys_facessat(Context* ctx){ // int dirfd, const char *pathname, int mode, int flags
  uintptr_t dirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t pathname = argraw(1, ctx, ARG_BUF);
#ifdef SYSCALL_DEBUG
  printf("TODO: facessat fd=%d pathname=%s\n", dirfd, pathname);
#endif
  return 0;
}

int sys_set_tid_address(Context* ctx){ // int* tidptr
  // uintptr_t tidptr = ;
  return kmt->gettask()->pid;
}

int sys_rt_sigprocmask(Context* ctx){ //int how, const kernel_sigset_t *set, kernel_sigset_t *oldset, size_t sigsetsize
#ifdef SYSCALL_DEBUG
  printf("TODO: sys_rt_sigprocmask\n");
#endif
  return 0;
}

int sys_rt_sigaction(Context* ctx){
#ifdef SYSCALL_DEBUG
  printf("TODO: sys_rt_sigaction\n");
#endif
  return 0;
}

int sys_rt_sigtimedwait(Context* ctx){
#ifdef SYSCALL_DEBUG
  printf("TODO: sys_rt_sigtimedwait\n");
#endif
  return 0;
}

int sys_clone(Context* ctx){ // unsigned long flags, void *child_stack, void *ptid, void *ctid, struct pt_regs *regs
  uintptr_t flags = argraw(0, ctx, ARG_NUM);
  uintptr_t child_stack = argraw(1, ctx, ARG_NUM);
  uintptr_t ptid = argraw(2, ctx, ARG_NUM);
  uintptr_t ctid = argraw(3, ctx, ARG_NUM);
  Assert(flags==17, "flags 0x%lx!= sig_chld\n", flags);
  return uproc->fork(flags);
}

int sys_wait4(Context* ctx){ // pid_t pid, int *wstatus, int options, struct rusage *rusage
  uintptr_t pid = argraw(0, ctx, ARG_NUM);
  uintptr_t wstatus = argraw(1, ctx, ARG_NUM);
  uintptr_t options = argraw(2, ctx, ARG_NUM);
  uintptr_t rusage = argraw(3, ctx, ARG_NUM);
  task_t* cur_task = kmt->gettask();
  cur_task->states[cur_task->int_depth + 1] = TASK_WAIT;
  yield();
  if(wstatus) copy_to_user(ctx, &(cur_task->wstatus), wstatus, sizeof(int));
  return pid;
}

int sys_prlimit64(Context* ctx){ // pid_t pid, int resource, const struct rlimit *new_limit, struct rlimit *old_limit
  uintptr_t pid = argraw(0, ctx, ARG_NUM);
  uintptr_t resource = argraw(1, ctx, ARG_NUM);
  uintptr_t new_limit = argraw(2, ctx, ARG_NUM);
  uintptr_t old_limit = argraw(3, ctx, ARG_NUM);
#ifdef SYSCALL_DEBUG
  printf("TODO: prlimit64 pid=0x%lx resource=0x%lx new_limit=0x%lx old_limit=0x%lx\n", pid, resource, new_limit, old_limit);
#endif
  return 0;
}

int sys_exit_group(Context* ctx){ // int status
  int status = argraw(0, ctx, ARG_NUM);
  return uproc->exit_group(status);
}

uint64_t r_time();

int sys_clock_gettime(Context* ctx){ // int clockid struct timespec* tp
  uintptr_t tp = argraw(1, ctx, ARG_PTR);
  uintptr_t time = r_time();
  *(uintptr_t*)tp = time / 1000;
  *(uintptr_t*)(tp+8) = time % 1000;
  return 0;
}

int sys_mprotect(Context* ctx){
  return 0;
}

int sys_utimenstat(Context* ctx){
  int dirfd = argraw(0, ctx, ARG_NUM);
  char* pathname = argraw(1, ctx, ARG_BUF);
  int fd = vfs->openat(dirfd, pathname, 0);
  if(fd > 0){
    vfs->close(fd);
    return 0;
  } else{
    return -ENOENT;
  }

  return -1;
}

int sys_unlinkat(Context* ctx){ // int dirfd, char* pathname, int flags
  uintptr_t dirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t pathname = argraw(1, ctx, ARG_BUF);
  uintptr_t flags = argraw(2, ctx, ARG_NUM);
  vfs->unlinkat(dirfd, pathname, flags);
}

int sys_getuid(Context* ctx){
  return 0;
}

int sys_geteuid(Context* ctx){
  return 0;
}

int sys_getegid(Context* ctx){
  return 0;
}

int sys_getgid(Context* ctx){
  return 0;
}

int sys_getppid(Context* ctx){
  return kmt->gettask()->ppid;
}

int sys_uname(Context* ctx){ // struct utsname *buf

  uintptr_t buf = argraw(0, ctx, ARG_NUM);
  char* sysname = "Linux";
  char* nodename = "somename";
  char* release = "4.15.0";
  char* version = "v1.0.0";
  char* machine = "somemachine";
  char* domainname = "somedomain";
  copy_to_user(ctx, sysname,    buf,          strlen(sysname));
  copy_to_user(ctx, nodename,   buf + 65,     strlen(nodename));
  copy_to_user(ctx, release,    buf + 65 * 2, strlen(release));
  copy_to_user(ctx, version,    buf + 65 * 3, strlen(version));
  copy_to_user(ctx, machine,    buf + 65 * 4, strlen(machine));
  copy_to_user(ctx, domainname, buf + 65 * 5, strlen(domainname));
  return 0;
}

int sys_ioctl(Context* ctx){
  uintptr_t fd = argraw(0, ctx, ARG_NUM);
  uintptr_t request = argraw(1, ctx, ARG_NUM);
  uintptr_t dst = argraw(2, ctx, ARG_NUM);
  switch(request){
    case 0x5413:
      break;
    default: Assert(0, "invalid request 0x%lx\n", request);
  }
  return 0;
}

int sys_writev(Context* ctx){ // int fd, const struct iovec *iov, int iovcnt
  uintptr_t fd = argraw(0, ctx, ARG_NUM);
  uintptr_t iov_addr = argraw(1, ctx, ARG_NUM);
  uintptr_t iovcnt = argraw(2, ctx, ARG_NUM);
  iovec iov[iovcnt];
  copy_from_user(ctx, iov, iov_addr, sizeof(iovec) * iovcnt);
  int ret = 0;
  for(int i = 0; i < iovcnt; i++){
    char buf[iov[i].iov_len];
    copy_from_user(ctx, buf, iov[i].iov_base, iov[i].iov_len);
    vfs->write(fd, (void*)buf, iov[i].iov_len);
    ret += iov[i].iov_len;
  }

  return ret;
}

int sys_readv(Context* ctx){ // int fd, const struct iovec *iov, int iovcnt
  uintptr_t fd = argraw(0, ctx, ARG_NUM);
  uintptr_t iov_addr = argraw(1, ctx, ARG_NUM);
  uintptr_t iovcnt = argraw(2, ctx, ARG_NUM);
  iovec iov[iovcnt];
  copy_from_user(ctx, iov, iov_addr, sizeof(iovec) * iovcnt);
  int ret = 0;
  for(int i = 0; i < iovcnt; i++){
    char buf[iov[i].iov_len];
    int count = MIN(vfs->read(fd, (void*)buf, iov[i].iov_len), iov[i].iov_len);
    copy_to_user(ctx, buf, iov[i].iov_base, count);
    ret += count;
  }
  return ret;
}

int sys_statfs(Context* ctx){ // const char *path, struct statfs *buf
  uintptr_t path = argraw(0, ctx, ARG_BUF);
  uintptr_t buf = argraw(1, ctx, ARG_NUM);
  statfs stat;
  vfs->statfs(path, &stat);
  copy_to_user(ctx, &stat, buf, sizeof(statfs));
  return 0;
}

int sys_syslog(Context* ctx){ // int type, char *bufp, int len
  uintptr_t type = argraw(0, ctx, ARG_NUM);
  uintptr_t bufp = argraw(1, ctx, ARG_NUM);
  uintptr_t len = argraw(2, ctx, ARG_NUM);
  switch(type){
    case SYSLOG_ACTION_READ_ALL: return 0;
    case SYSLOG_ACTION_SIZE_BUFFER: return 64;
    default: Assert(0, "invalid type %d\n", type);
  }
}

int sys_fstatat(Context* ctx){ // int dirfd, const char *pathname, struct stat *statbuf, int flags
  uintptr_t dirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t pathname = argraw(1, ctx, ARG_BUF);
  uintptr_t statbuf = argraw(2, ctx, ARG_NUM);
  uintptr_t flags = argraw(3, ctx, ARG_NUM);
  stat stat;
  int ret = vfs->fstatat(dirfd, pathname, &stat, flags);
  if(ret == 0) copy_to_user(ctx, &stat, statbuf, sizeof(statfs));
  return ret;
}

int sys_faccessat(Context* ctx){ // int dirfd, const char *pathname, int mode, int flags
  return 0;
}

int sys_sysinfo(Context* ctx){ // struct sysinfo *info
  return 0;
}

int sys_fcntl(Context* ctx){ // int fd, int cmd
  int fd = argraw(0, ctx, ARG_NUM);
  int cmd = argraw(1, ctx, ARG_NUM);
  uintptr_t arg = argraw(2, ctx, ARG_NUM);
  switch(cmd){
    case F_GETFD: return 0;
    case F_SETFD: return 0; // TODO: CLOEXEC
    case F_GETFL: return kmt->gettask()->ofiles[fd]->flag;
    default: Assert(0, "invalid fcntl cmd=0x%lx", cmd);
  }
}

int sys_readlinkat(Context* ctx){ // int dirfd, const char *pathname, char *buf, size_t bufsiz
  uintptr_t dirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t pathname = argraw(1, ctx, ARG_BUF);
  uintptr_t buf = argraw(2, ctx, ARG_NUM);
  uintptr_t bufsz = argraw(3, ctx, ARG_NUM);
  int copy_size = MIN(bufsz, strlen((void*)pathname));
  copy_to_user(ctx, pathname, buf, copy_size);
  return copy_size;
}

int sys_getdents(Context* ctx){ // unsigned int fd, struct linux_dirent *dirp,  unsigned int count
  int fd = argraw(0, ctx, ARG_NUM);
  uintptr_t dirp = argraw(1, ctx, ARG_NUM);
  int count = argraw(2, ctx, ARG_NUM);
  void* buf = pmm->alloc(count);
  int ret = vfs->getdent(fd, buf, count);
  copy_to_user(ctx, buf, dirp, ret);
  pmm->free(buf);
  return ret;
}

int sys_kill(Context* ctx){
  int pid = argraw(0, ctx, ARG_NUM);
  Assert(task_by_pid(pid) == NULL, "pid %d is not empty", pid);
  return 0;
}

int sys_nanosleep(Context* ctx){
  return 0;
}

int sys_set_robust_list(Context* ctx){
  return 0;
}

int sys_sendfile(Context* ctx){ // int out_fd, int in_fd, off_t *offset, size_t count
  int outfd = argraw(0, ctx, ARG_NUM);
  int infd = argraw(1, ctx, ARG_NUM);
  uintptr_t offset = argraw(2, ctx, ARG_NUM);
  uintptr_t count = argraw(3, ctx, ARG_NUM);
  Assert(offset == 0, "sendfile offset 0x%lx is not NULL", offset);
  int ret = 0;
  char buf[32];
  while(count){
    int copy_size = MIN(count, 32);
    int tmp = vfs->read(infd, buf, copy_size);
    if(!tmp) return
    ret += tmp;
    vfs->write(outfd, buf, tmp);
  }
  return ret;
}

int sys_dup3(Context* ctx){ // int oldfd, int newfd, int flags
  int oldfd = argraw(0, ctx, ARG_NUM);
  int newfd = argraw(1, ctx, ARG_NUM);
  int flags = argraw(2, ctx, ARG_NUM);
  if(oldfd == newfd) return newfd;
  task_t* task = kmt->gettask();
  pushcli();
  if(task->ofiles[newfd]){
    fileclose(task->ofiles[newfd]);
  }
  task->ofiles[newfd] = filedup(task->ofiles[oldfd]);
  popcli();
  return newfd;
}

int sys_renameat2(Context* ctx){ // int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags
  int olddirfd = argraw(0, ctx, ARG_NUM);
  uintptr_t oldpath = argraw(1, ctx, ARG_BUF);
  int newdirfd = argraw(2, ctx, ARG_NUM);
  uintptr_t newpath = argraw(3, ctx, ARG_BUF);
  uintptr_t flags = argraw(4, ctx, ARG_NUM);
  return vfs->renameat2(olddirfd, oldpath, newdirfd, newpath, flags);
}

int sys_getrusage(Context* ctx){ // int who, struct rusage *usage
  uintptr_t usage = argraw(1, ctx, ARG_NUM);
  rusage_t rusage = {
    .ru_utime_sec = 1, .ru_utime_usec = 1,
    .ru_stime_sec = 1, .ru_stime_usec = 1,
  };
  copy_to_user(ctx, &rusage, usage, sizeof(rusage_t));
  return 0;
}

int sys_pipe2(Context* ctx){ // int pipefd[2], int flag
  uintptr_t pipe_addr = argraw(0, ctx, ARG_NUM);
  uintptr_t flags = argraw(1, ctx, ARG_NUM);
  int pipefd[2];
  int ret = vfs->pipe2(pipefd, flags);
  copy_to_user(ctx, pipefd, pipe_addr, sizeof(int) * 2);
  return ret;
}

int sys_pselect6(Context* ctx){
  return 0;
}

int sys_setitimer(Context* ctx){
  return 0;
}

int sys_umask(Context* ctx){
  return 0;
}

static int (*syscalls[MAX_SYSCALL_IDX])() = {
[SYS_chdir]     = sys_chdir,
[SYS_close]     = sys_close,
[SYS_dup]       = sys_dup,
[SYS_execve]    = sys_execve,
[SYS_exit]      = sys_exit,
[SYS_fstat]     = sys_fstat,
// [SYS_link]      = sys_link,
[SYS_lseek]     = sys_lseek,
[SYS_mkdirat]     = sys_mkdirat,
[SYS_mmap]      = sys_mmap,
[SYS_openat]      = sys_openat,
[SYS_read]      = sys_read,
[SYS_write]     = sys_write,
[SYS_brk]       = sys_brk,
[SYS_getpid]    = sys_getpid,
[SYS_getcwd]    = sys_getcwd,
[SYS_set_tid_address] = sys_set_tid_address,
[SYS_rt_sigprocmask] = sys_rt_sigprocmask,
[SYS_rt_sigaction] = sys_rt_sigaction,
[SYS_rt_sigtimedwait] = sys_rt_sigtimedwait,
[SYS_clone] = sys_clone,
[SYS_wait4] = sys_wait4,
[SYS_gettid] = sys_gettid,
[SYS_prlimit64] = sys_prlimit64,
[SYS_exit_group] = sys_exit_group,
[SYS_clock_gettime] = sys_clock_gettime,
[SYS_mprotect] = sys_mprotect,
[SYS_utimenstat] = sys_utimenstat,
[SYS_unlinkat] = sys_unlinkat,
[SYS_getuid] = sys_getuid,
[SYS_munmap] = sys_munmap,
[SYS_getppid] = sys_getppid,
[SYS_geteuid] = sys_geteuid,
[SYS_getgid] = sys_getgid,
[SYS_getegid] = sys_getegid,
[SYS_uname] = sys_uname,
[SYS_ioctl] = sys_ioctl,
[SYS_writev] = sys_writev,
[SYS_statfs] = sys_statfs,
[SYS_syslog] = sys_syslog,
[SYS_fstatat] = sys_fstatat,
[SYS_faccessat] = sys_faccessat,
[SYS_sysinfo] = sys_sysinfo,
[SYS_fcntl] = sys_fcntl,
[SYS_readlinkat] = sys_readlinkat,
[SYS_getdents] = sys_getdents,
[SYS_kill] = sys_kill,
[SYS_nanosleep] = sys_nanosleep,
[SYS_set_robust_list] = sys_set_robust_list,
[SYS_sendfile] = sys_sendfile,
[SYS_readv] = sys_readv,
[SYS_dup3] = sys_dup3,
[SYS_renameat2] = sys_renameat2,
[SYS_getrusage] = sys_getrusage,
[SYS_pipe2] = sys_pipe2,
[SYS_pselect6] = sys_pselect6,
[SYS_setitimer] = sys_setitimer,
[SYS_umask] = sys_umask,
// [SYS_faccessat] = sys_facessat,
};

Context* do_syscall(Event ev, Context* context){
  iset(true);
  int syscall_no = context->gpr[17];
  Assert(syscall_no < MAX_SYSCALL_IDX, "invalid syscall 0x%x at pc 0x%lx\n", syscall_no, context->epc);
  int (*sys_handler)() = syscalls[syscall_no];
  if(!sys_handler){
    void disp_ctx(Event* ev, Context* ctx);
    disp_ctx(&ev, context);
  }
  Assert(sys_handler, "invalid syscall %d at pc 0x%lx\n", syscall_no, context->epc);
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

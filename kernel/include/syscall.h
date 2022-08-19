#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H

#include <errno.h>

#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_getpid 172
#define SYS_getppid 173
#define SYS_kill 129
#define SYS_read 63
#define SYS_write 64
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_brk 214
#define SYS_linkat 37
#define SYS_unlinkat 35
#define SYS_mkdirat 34
#define SYS_renameat 38
#define SYS_chdir 49
#define SYS_getcwd 17
#define SYS_fstat 80
#define SYS_fstatat 79
#define SYS_faccessat 48
#define SYS_pread 67
#define SYS_pwrite 68
#define SYS_uname 160
#define SYS_getuid 174
#define SYS_geteuid 175
#define SYS_getgid 176
#define SYS_getegid 177
#define SYS_gettid 178
#define SYS_mmap 222
#define SYS_munmap 215
#define SYS_mremap 216
#define SYS_mprotect 226
#define SYS_prlimit64 261
#define SYS_getmainvars 2011
#define SYS_rt_sigaction 134
#define SYS_writev 66
#define SYS_gettimeofday 169
#define SYS_times 153
#define SYS_fcntl 25
#define SYS_ftruncate 46
#define SYS_getdents 61
#define SYS_dup 23
#define SYS_readlinkat 78
#define SYS_rt_sigprocmask 135
#define SYS_ioctl 29
#define SYS_getrlimit 163
#define SYS_setrlimit 164
#define SYS_getrusage 165
#define SYS_clock_gettime 113
#define SYS_set_tid_address 96
#define SYS_set_robust_list 99
#define SYS_execve 221
#define SYS_clone 220
#define SYS_rt_sigtimedwait 137
#define SYS_wait4 260
#define SYS_prlimit64 261
#define SYS_exit_group 94
#define SYS_utimenstat 88
#define SYS_unlinkat 35
#define SYS_statfs 43
#define SYS_syslog 116
#define SYS_sysinfo 179
#define SYS_fcntl 25
#define SYS_nanosleep 101
#define SYS_sendfile 71
#define SYS_readv 65
#define SYS_dup3 24
#define SYS_renameat2 276
#define SYS_tgkill 131
#define SYS_pipe2 59
#define SYS_pselect6 72
#define SYS_setitimer 103
#define SYS_umask 166

#define MAX_SYSCALL_IDX 0x400

#endif

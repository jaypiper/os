#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H

#define SYS_READ      0x0
#define SYS_WRITE     0x1
#define SYS_OPEN      0x2
#define SYS_CLOSE     0x3
#define SYS_FSTAT     0x5
#define SYS_LSEEK     0x8
#define SYS_MMAP      0x9
#define SYS_DUP       0x20
#define SYS_FORK      0x39
#define SYS_EXECVE    0x3b
#define SYS_EXIT      0x3c
#define SYS_CHDIR     0x50
#define SYS_MKDIR     0x53
#define SYS_LINK      0x56
#define SYS_UNLINK    0x57

#define MAX_SYSCALL_IDX 0x100

#endif

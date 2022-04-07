#include <sys/syscall.h>
#include "testlib.h"

int _start(){
  char* str = "hello os!\n";
  int len = 10;
  do_syscall3(SYS_write, 1, (unsigned long long)str, len);
  char* str2 = "/hello";
  do_syscall3(SYS_execve, (unsigned long long)str2, 0, 0);
  do_syscall1(SYS_exit, 0);
}
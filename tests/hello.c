#include <sys/syscall.h>
#include "testlib.h"

int _start(){
  char* str = "hello os!\n";
  int len = 10;
  do_syscall3(SYS_write, 1, (unsigned long long)str, len);
  do_syscall1(SYS_exit, 0);
}
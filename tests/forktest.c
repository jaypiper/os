#include <sys/syscall.h>
#include "testlib.h"

int _start(){
  char* str = "hello os!\n";
  int len = 10;
  do_syscall3(SYS_write, 1, (unsigned long long)str, len);
  int pid = do_syscall1(SYS_fork, 0);
  if(pid == 0){
    char* str_pid0 = "pid 0\n";
    do_syscall3(SYS_write, 1, (unsigned long long)str_pid0, 6);
  }else{
    char* str_pidn0 = "pid not 0\n";
    do_syscall3(SYS_write, 1, (unsigned long long)str_pidn0, len);
  }
  char* str2 = "/hello";
  do_syscall3(SYS_execve, (unsigned long long)str2, 0, 0);
  do_syscall1(SYS_exit, 0);
}
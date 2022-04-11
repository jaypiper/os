#include <sys/syscall.h>
#include "testlib.h"

#define N  10

char buf[32] ;

void print(const char *s){
  do_syscall3(SYS_write, 1, (unsigned long long)s, my_strlen(s));
}

void forktest() {
  buf[0] = 'p'; buf[1] = 'i'; buf[2] = 'd'; buf[3] = ' ';
  print("fork test\n");

  int n = 0, pid = 0;

  for(n=0; n<N; n++){
    pid = do_syscall1(SYS_fork, 0);
    if(pid == 0){
      buf[4] = '0'; buf[5] = '\n'; buf[6] = 0;
    }else{
      char* buf_end = print_num(buf + 4, pid, 10);
      buf_end[0] = '\n';
      buf_end[1] = 0;
    }
    print(buf);

    if(pid == 0)
      do_syscall1(SYS_exit, 0);
  }

  print("fork test OK\n");
  do_syscall1(SYS_exit, 0);
}

int
_start()
{
  forktest();
  do_syscall1(SYS_exit, 0);
}

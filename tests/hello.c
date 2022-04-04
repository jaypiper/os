#include <sys/syscall.h>



void do_syscall3(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3){

  asm volatile("movq %0, %%rdi; \
                movq %1, %%rsi; \
                movq %2, %%rdx; \
                movq %3, %%rax; \
                int $0x80" : : "r"(val1), "r"(val2), "r"(val3), "r"((unsigned long long)syscall) : "%rdi", "%rsi", "%rdx", "%rax");

}

void do_syscall1(int syscall, long long val1){
  asm volatile("movq %0, %%rdi; \
                movq %1, %%rax; \
                int $0x80" : : "r"(val1), "r"((unsigned long long)syscall));
}

int _start(){
  char* str = "hello os!\n";
  int len = 10;
  do_syscall3(SYS_write, 1, (unsigned long long)str, len);
  do_syscall1(SYS_exit, 0);
}
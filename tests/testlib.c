int do_syscall3(int syscall, unsigned long long val1, unsigned long long val2, unsigned long long val3){
  unsigned long long ret;
  asm volatile("movq %1, %%rdi; \
                movq %2, %%rsi; \
                movq %3, %%rdx; \
                movq %4, %%rax; \
                int $0x80; \
                movq %%rax, %0" : "=r"(ret) : "r"(val1), "r"(val2), "r"(val3), "r"((unsigned long long)syscall) : "%rdi", "%rsi", "%rdx", "%rax");
  return ret;
}

int do_syscall1(int syscall, long long val1){
  unsigned long long ret;
  asm volatile("movq %1, %%rdi; \
                movq %2, %%rax; \
                int $0x80; \
                movq %%rax, %0" : "=r"(ret) : "r"(val1), "r"((unsigned long long)syscall));
  return ret;
}
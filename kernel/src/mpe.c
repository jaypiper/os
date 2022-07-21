#include <am.h>
#include <riscv64.h>

// struct cpu_local __am_cpuinfo[MAX_CPU] = {};
static void (* volatile user_entry)();
// static int ap_ready = 0;

bool mpe_init(void (*entry)()) {
  return true;
}

int cpu_count() {
  return NCPU;
}

int cpu_current() {
  return 0;
  uint32_t x;
  r_gpr("tp", x);
  return x;
}

int atomic_xchg(int *addr, int newval) {
  __sync_synchronize();
  int result = __sync_lock_test_and_set(addr, newval);
  __sync_synchronize();
  return result;
}

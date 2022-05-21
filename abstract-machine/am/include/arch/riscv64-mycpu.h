#ifndef ARCH_H__
#define ARCH_H__

struct Context {
  uintptr_t gpr[32];
  uintptr_t epc;
  uintptr_t cause;
  uintptr_t status;
  uintptr_t satp;
  uintptr_t kernel_satp;
  uintptr_t kernel_sp;
  uintptr_t kernel_trap;
};

#define GPR1 gpr[17]
#define GPR2 gpr[10]
#define GPR3 gpr[11]
#define GPR4 gpr[12]
#define GPRx gpr[10]


#define EPC_OFFSET    (32 * 8)
#define CAUSE_OFFSET  (33 * 8)
#define STATUS_OFFSET (34 * 8)
#define SATP_OFFSET   (35 * 8)
#define KSATP_OFFSET  (36 * 8)
#define KSP_OFFSET    (37 * 8)
#define KTRAP_OFFSET  (38 * 8)


#define bug_on(cond) \
  do { \
    if (cond) panic("internal error (likely a bug in AM)"); \
  } while (0)

#define bug() bug_on(1)

#endif

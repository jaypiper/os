#ifndef RISCV_H
#define RISCV_H

#define w_gpr(r, val) asm volatile("mv " r ",%0" : : "r"(val))
#define r_gpr(r, dst) asm volatile("mv %0, " r : : "r"(dst))
#define w_csr(r, val) asm volatile("csrw " r ", %0" : : "r" (val));
#define r_csr(r, dst) asm volatile("csrr %0, " r : "=r"(dst))

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)    // machine-mode interrupt enable.

#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))

#define NO_RA 1
#define NO_SP 2
#define NO_TP 4
#define NO_A0 10

#endif

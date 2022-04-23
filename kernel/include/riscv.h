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

// Supervisor Status Register, sstatus
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64_t)pagetable) >> 12))

#define NO_RA 1
#define NO_SP 2
#define NO_TP 4
#define NO_A0 10

#endif

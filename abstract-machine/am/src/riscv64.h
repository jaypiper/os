#ifndef RISCV64_H__
#define RISCV64_H__

#include <stdint.h>

static inline uint8_t  inb(uintptr_t addr) { return *(volatile uint8_t  *)addr; }
static inline uint16_t inw(uintptr_t addr) { return *(volatile uint16_t *)addr; }
static inline uint32_t inl(uintptr_t addr) { return *(volatile uint32_t *)addr; }

static inline void outb(uintptr_t addr, uint8_t  data) { *(volatile uint8_t  *)addr = data; }
static inline void outw(uintptr_t addr, uint16_t data) { *(volatile uint16_t *)addr = data; }
static inline void outl(uintptr_t addr, uint32_t data) { *(volatile uint32_t *)addr = data; }

#define PTE_V 0x01
#define PTE_R 0x02
#define PTE_W 0x04
#define PTE_X 0x08
#define PTE_U 0x10


#define w_gpr(r, val) asm volatile("mv " r ",%0" : : "r"(val))
#define r_gpr(r, dst) asm volatile("mv %0, " r : "=r"(dst))
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


// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

#define CAUSE_MISALIGNED_FETCH            0x0
#define CAUSE_FETCH_ACCESS                0x1
#define CAUSE_ILLEGAL_INSTRUCTION         0x2
#define CAUSE_BREAKPOINT                  0x3
#define CAUSE_MISALIGNED_LOAD             0x4
#define CAUSE_LOAD_ACCESS                 0x5
#define CAUSE_MISALIGNED_STORE            0x6
#define CAUSE_STORE_ACCESS                0x7
#define CAUSE_USER_ECALL                  0x8
#define CAUSE_SUPERVISOR_ECALL            0x9
#define CAUSE_VIRTUAL_SUPERVISOR_ECALL    0xa
#define CAUSE_MACHINE_ECALL               0xb
#define CAUSE_FETCH_PAGE_FAULT            0xc
#define CAUSE_LOAD_PAGE_FAULT             0xd
#define CAUSE_STORE_PAGE_FAULT            0xf
#define CAUSE_FETCH_GUEST_PAGE_FAULT      0x14
#define CAUSE_LOAD_GUEST_PAGE_FAULT       0x15
#define CAUSE_VIRTUAL_INSTRUCTION         0x16
#define CAUSE_STORE_GUEST_PAGE_FAULT      0x17

#define IRQ_U_SOFT   0
#define IRQ_S_SOFT   1
#define IRQ_VS_SOFT  2
#define IRQ_M_SOFT   3
#define IRQ_U_TIMER  4
#define IRQ_S_TIMER  5
#define IRQ_VS_TIMER 6
#define IRQ_M_TIMER  7
#define IRQ_U_EXT    8
#define IRQ_S_EXT    9
#define IRQ_VS_EXT   10
#define IRQ_M_EXT    11
#define IRQ_S_GEXT   12
#define IRQ_COP      12
#define IRQ_HOST     13
#define INVALID_IRQ  63

#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uintptr_t)pagetable) >> 12))

#define MAX_CPU 8

#endif

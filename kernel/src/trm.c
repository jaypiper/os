#include <am.h>
#include <klib-macros.h>
#include <uart.h>
#include <riscv64.h>
#include <device.h>
extern char _heap_start;
int main(const char *args);

extern char _addr_start;
#define PMEM_SIZE (6 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_addr_start + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
#ifndef MAINARGS
#define MAINARGS ""
#endif
static const char mainargs[] = MAINARGS;

void putch(char ch) {
  uarths_putchar(ch);
}

void halt(int code) {
  asm volatile("mv a0, %0; .word 0x0000006b" : :"r"(code));
  while (1);
}

#define OS

uint64_t timer_scratch[MAX_CPU][5];
extern void timervec(void);

void timerinit() {
  // each CPU has a separate source of timer interrupts.
  int id;
  r_csr("mhartid", id);

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64_t*)CLINT_MTIMECMP(id) = *(uint64_t*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64_t *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;

  w_csr("mscratch", (uint64_t)scratch);

  // set the machine-mode trap handler.
  w_csr("mtvec", (uint64_t)timervec);

  // enable machine-mode interrupts.
  uint64_t status;
  r_csr("mstatus", status);
  w_csr("mstatus", status | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  uint64_t mie;
  r_csr("mie", mie);
  w_csr("mie", mie | MIE_MTIE);
}


void _trm_init() {

  // virt_uart_init();
#ifdef OS
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x;
  r_csr("mstatus", x);
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_csr("mstatus", x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_csr("mepc", (uint64_t)main);

  // disable paging for now.
  w_csr("satp", 0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_csr("medeleg", 0xffff);
  w_csr("mideleg", 0xffff);

  r_csr("sie", x);
  w_csr("sie", x | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  // w_csr("pmpaddr0", 0x3fffffffffffffull);
  // w_csr("pmpcfg0", 0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id;
  r_csr("mhartid", id);
  w_gpr("tp", id);

  asm volatile ("mret");
#endif

  int ret = main(mainargs);
  halt(ret);
}

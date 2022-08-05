#include <am.h>
#include <klib-macros.h>
#include <uart.h>
#include <riscv64.h>
#include <device.h>
extern char _heap_start;
int main(const char *args);

extern char _addr_start;
#define PMEM_SIZE (6 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)0x80000000 + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
#ifndef MAINARGS
#define MAINARGS ""
#endif
static const char mainargs[] = MAINARGS;

void putch(char ch) {
#ifdef PLATFORM_QEMU
  drv_uart_putc(ch);
#else
  uarths_putchar(ch);
#endif
}

void halt(int code) {
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


void _trm_init(int hartid) {

  // virt_uart_init();
#ifdef OS
  w_gpr("tp", hartid);
#endif

  int ret = main(mainargs);
  halt(ret);
}

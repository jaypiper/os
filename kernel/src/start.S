.section entry, "ax"
.globl _start
.type _start, @function

_start:
  mv s0, zero
  csrr tp, mhartid
  beqz tp, cpu0_init
  j cpu1_init

cpu0_init:
  la sp, _stack_pointer
  jal _trm_init


cpu1_init:
  j cpu1_init
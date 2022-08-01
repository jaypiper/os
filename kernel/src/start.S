.section entry, "ax"
.globl _start
.type _start, @function

_start:
  bnez a0, loop
  mv s0, zero
  la sp, _stack_pointer
  slli t0, a0, 12
  add sp, sp, t0

  jal _trm_init

loop:
  j loop
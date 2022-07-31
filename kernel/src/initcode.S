.section .data
.globl initcode_start, initcode_end
.align 12
initcode_start:
.incbin "../../initcode/build/initcode-riscv64.bin"
initcode_end:
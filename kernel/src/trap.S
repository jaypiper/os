#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define MAP(c, f) c(f)

#define REGS(f) \
      f( 1) f( 2) f( 3) f( 4) f( 5) f( 6) f( 7) f( 8) f( 9) \
			f(11) f(12) f(13) f(14) f(15) f(16) f(17) f(18) f(19) \
f(20) f(21) f(22) f(23) f(24) f(25) f(26) f(27) f(28) f(29) \
f(30) f(31) f(10)

#define PUSH(n, reg) sd concat(x, n), (n * 8)(reg);
#define POP(n, reg)  ld concat(x, n), (n * 8)(reg);

#define PUSH_SP(n) PUSH(n, sp)
#define POP_SP(n) POP(n, sp)
#define PUSH_A0(n) PUSH(n, a0)
#define POP_A0(n) POP(n, a0)

#define EPC_OFFSET    (32 * 8)
#define CAUSE_OFFSET  (33 * 8)
#define STATUS_OFFSET (34 * 8)
#define SATP_OFFSET   (35 * 8)
#define KSATP_OFFSET  (36 * 8)
#define KSP_OFFSET    (37 * 8)
#define KTRAP_OFFSET  (38 * 8)
#define CONTEXT_SIZE 	((32 + 7) * 8)

# kernel_trap: only gprs are saved and recovered
.globl kernel_trap
.align 4
kernel_trap:
	addi sp, sp, -CONTEXT_SIZE
	MAP(REGS, PUSH_SP)

	# store the original sp
	addi t0, sp, CONTEXT_SIZE
	sd t0, 16(sp)

	la t0, kernel_trap
	sd t0, KTRAP_OFFSET(sp)

	# save epc, cause, status
	csrr t0, sepc
	sd t0, EPC_OFFSET(sp)
	csrr t0, scause
	sd t0, CAUSE_OFFSET(sp)
	csrr t0, sstatus
	sd t0, STATUS_OFFSET(sp)
	csrr t0, satp
	sd t0, SATP_OFFSET(sp)

	csrw sscratch, sp

	mv a0, sp
	jal __am_irq_handle

	# store context
	csrw sscratch, a0

	# restore epc, cause, status
	ld t0, EPC_OFFSET(a0)
	csrw sepc, t0
	ld t0, CAUSE_OFFSET(a0)
	csrw scause, t0
	ld t0, STATUS_OFFSET(a0)
	csrw sstatus, t0

	ld t0, KTRAP_OFFSET(a0)
	csrw stvec, t0

	ld t0, SATP_OFFSET(a0)
	csrw satp, t0
	sfence.vma zero, zero


	MAP(REGS, POP_A0)

	sret


#user_trap: task Context is stored in sscratch
.global user_trap
.align 4
user_trap:

	csrrw a0, sscratch, a0

	MAP(REGS, PUSH_A0)

	#save a0
	csrr t0, sscratch
	sd t0, 10*8(a0)

	# save epc, cause, status
	csrr t0, sepc
	sd t0, EPC_OFFSET(a0)
	csrr t0, scause
	sd t0, CAUSE_OFFSET(a0)
	csrr t0, sstatus
	sd t0, STATUS_OFFSET(a0)
	csrr t0, satp
	sd t0, SATP_OFFSET(a0)

	# switch to kernel stack
	ld sp, KSP_OFFSET(a0)

	# restore kernel pagetable
	# ld t0, KSATP_OFFSET(a0)
	# csrw satp, t0
	# sfence.vma zero, zero

	# send interrupt & exception to kernel_trap
	la t0, kernel_trap
	csrw stvec, t0

	csrw sscratch, a0

	jal __am_irq_handle

	csrw sscratch, a0

	ld t0, KTRAP_OFFSET(a0)
	csrw stvec, t0

	# restore user pagetable
	ld t0, SATP_OFFSET(a0)
	csrw satp, t0
	sfence.vma zero, zero

	# restore epc, cause, status
	ld t0, EPC_OFFSET(a0)
	csrw sepc, t0
	ld t0, CAUSE_OFFSET(a0)
	csrw scause, t0
	ld t0, STATUS_OFFSET(a0)
	csrw sstatus, t0

	MAP(REGS, POP_A0)

	sret


.globl timervec
.align 4
timervec:
  # start.c has set up the memory that mscratch points to:
  # scratch[0,8,16] : register save area.
  # scratch[24] : address of CLINT's MTIMECMP register.
  # scratch[32] : desired interval between interrupts.
  
  csrrw a0, mscratch, a0
  sd a1, 0(a0)
  sd a2, 8(a0)
  sd a3, 16(a0)

  # schedule the next timer interrupt
  # by adding interval to mtimecmp.
  ld a1, 24(a0) # CLINT_MTIMECMP(hart)
  ld a2, 32(a0) # interval
  ld a3, 0(a1)
  add a3, a3, a2
  sd a3, 0(a1)

  # raise a supervisor software interrupt.
	li a1, 2
  csrw sip, a1

  ld a3, 16(a0)
  ld a2, 8(a0)
  ld a1, 0(a0)
  csrrw a0, mscratch, a0

  mret


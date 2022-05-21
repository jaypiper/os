.PHONY: build-arg

.DEFAULT_GOAL = build-arg

RV_ARCH       = rv64gc
CROSS_COMPILE := riscv64-linux-gnu-
COMMON_FLAGS  := -fno-pic -march=$(RV_ARCH) -mcmodel=medany
CFLAGS        += $(COMMON_FLAGS) -static
ASFLAGS       += $(COMMON_FLAGS) -O0
LDFLAGS       += -melf64lriscv

AM_SRCS := mycpu/start.S \
           mycpu/trm.c \
           mycpu/libgcc/muldi3.S \
           mycpu/libgcc/div.S \
           mycpu/ioe.c \
           mycpu/timer.c \
           mycpu/input.c \
           mycpu/cte.c \
           mycpu/trap.S \
           mycpu/vme.c \
           mycpu/mpe.c \
           mycpu/uart.c \
					 mycpu/disk.c \
					 mycpu/gpu.c

CFLAGS    += -fdata-sections -ffunction-sections
CFLAGS += -I$(AM_HOME)/am/src/mycpu/include
LDFLAGS   += -T $(AM_HOME)/scripts/mycpu.ld --defsym=_stack_pointer=0x80100000 --defsym=_pmem_start=0x80000000
ifdef FLASH
  LDFLAGS += --defsym=_addr_start=0x30000000
else
  LDFLAGS += --defsym=_addr_start=0x80000000
endif
LDFLAGS   += --gc-sections -e _start
CFLAGS += -DMAINARGS=\"$(mainargs)\" -DNCPU=1

build-arg: image
	( echo -n $(mainargs); ) | dd if=/dev/stdin of=$(IMAGE) bs=512 count=2 seek=1 conv=notrunc status=none
	make mkfs

image: $(IMAGE).elf
	@$(OBJDUMP) -d $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin
	@( cat $(IMAGE).bin; head -c 1024 /dev/zero) > $(IMAGE)


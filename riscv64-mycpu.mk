.PHONY: build-arg

.DEFAULT_GOAL = build-arg

RV_ARCH       = rv64gc
CROSS_COMPILE := riscv64-unknown-elf-
COMMON_FLAGS  := -fno-pic -march=$(RV_ARCH) -mcmodel=medany
CFLAGS        += $(COMMON_FLAGS) -static
ASFLAGS       += $(COMMON_FLAGS) -O0
#LDFLAGS       += -melf64lriscv

AM_SRCS := start.S \
           trm.c \
           libgcc/muldi3.S \
           libgcc/div.S \
           ioe.c \
           timer.c \
           input.c \
           cte.c \
           trap.S \
           vme.c \
           mpe.c \
           uart.c \
					 disk.c \
					 gpu.c

RUSTSBI_SIZE = 128k
RUSTSBI = ./bootloader/rustsbi-k210

CFLAGS    += -fdata-sections -ffunction-sections -fstrict-volatile-bitfields
CFLAGS += -I$(AM_HOME)/am/src/include
LDFLAGS   += -Wl,-T$(AM_HOME)/mycpu.ld -Wl,--defsym=_pmem_start=0x80000000
ifdef FLASH
  LDFLAGS += -Wl,--defsym=_addr_start=0x30000000
else
  LDFLAGS += -Wl,--defsym=_addr_start=0x80020000
endif
LDFLAGS   += -Wl,--gc-sections -Wl,-e_start
CFLAGS += -DMAINARGS=\"$(mainargs)\" -DNCPU=1

build-arg: image
	( echo -n $(mainargs); ) | dd if=/dev/stdin of=$(IMAGE) bs=512 count=2 seek=1 conv=notrunc status=none
	make mkfs

$(IMAGE).elf: $(OBJS) $(LIBS)
	echo fjksdjflks

image: $(IMAGE).elf
	@$(OBJDUMP) -d $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin
	@( cat $(IMAGE).bin; head -c 1024 /dev/zero) > $(IMAGE)

all: image
	$(OBJCOPY) $(RUSTSBI) --strip-all -O binary os.bin
	dd if=$(IMAGE).bin of=os.bin bs=$(RUSTSBI_SIZE) seek=1
	mkdir -p build
	$(OBJDUMP) -D -b binary -m riscv os.bin > build/os.asm

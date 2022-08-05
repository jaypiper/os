.PHONY: build-arg initcode

.DEFAULT_GOAL = build-arg

RV_ARCH       = rv64gc
CROSS_COMPILE := riscv64-unknown-elf-
COMMON_FLAGS  := -fno-pic -march=$(RV_ARCH) -mcmodel=medany
CFLAGS        += $(COMMON_FLAGS) -static
ASFLAGS       += $(COMMON_FLAGS) -O0
#LDFLAGS       += -melf64lriscv

PLATFORM ?= k210

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
LDFLAGS   += -Wl,-T$(AM_HOME)/mycpu.ld -Wl,--defsym=_pmem_start=0x80000000 -nostartfiles
ifeq ($(PLATFORM), k210)
  LDFLAGS += -Wl,--defsym=_addr_start=0x80020000
else
  LDFLAGS += -Wl,--defsym=_addr_start=0x80200000
  CFLAGS += -DPLATFORM_QEMU
endif
LDFLAGS   += -Wl,--gc-sections -Wl,-e_start
CFLAGS += -DMAINARGS=\"$(mainargs)\" -DNCPU=1

QEMU = ../../qemu/build/riscv64-softmmu/qemu-system-riscv64
QEMU-OPTS = -machine virt -kernel kernel-qemu
QEMU-OPTS += -m 128M -nographic -smp 2 -bios sbi-qemu
QEMU-OPTS += -drive file=sdcard.img,if=none,format=raw,id=x0
QEMU-OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMU-OPTS += -initrd initrd.img

build-arg: image
	( echo -n $(mainargs); ) | dd if=/dev/stdin of=$(IMAGE) bs=512 count=2 seek=1 conv=notrunc status=none
	make mkfs

$(IMAGE).elf: $(OBJS) $(LIBS)

$(shell rm build/riscv64-mycpu/kernel/src/initcode.o)

image: $(IMAGE).elf
	@$(OBJDUMP) -d $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin
	@( cat $(IMAGE).bin; head -c 1024 /dev/zero) > $(IMAGE)

initcode:
	make -C initcode build

run-qemu: all
	$(QEMU) $(QEMU-OPTS)

all: initcode image
ifeq ($(PLATFORM), qemu)
	cp bootloader/rustsbi-qemu sbi-qemu
	cp build/kernel-riscv64-mycpu.elf kernel-qemu
else
	$(OBJCOPY) $(RUSTSBI) --strip-all -O binary os.bin
	dd if=$(IMAGE).bin of=os.bin bs=$(RUSTSBI_SIZE) seek=1
	mkdir -p build
	$(OBJDUMP) -D -b binary -m riscv os.bin > build/os.asm
endif


SRCS =  $(shell find -L . -name "*.[c|S]")
KERNEL = $(shell pwd)/..
INC_PATH  := $(KERNEL)/include/ $(KERNEL)/kernel/framework/ $(KERNEL)/kernel/include $(KERNEL)/kernel/klib/include
INC_PATH  += board/k210/include

ARCH    := riscv64

RV_ARCH       = rv64gc
CROSS_COMPILE := riscv64-unknown-elf-
COMMON_FLAGS  := -fno-pic -march=$(RV_ARCH) -mcmodel=medany
NAME := initcode
LDFLAGS := -nostartfiles -Wl,-einitcode_test -T initcode.ld

AM_HOME = $(PWD)/..
include $(KERNEL)/AM_Makefile.mk

build: $(IMAGE).elf
	$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(IMAGE).elf $(IMAGE).bin

.PHONY: build
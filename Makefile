NAME      := kernel
SRCS      := $(shell find -L ./kernel/ -name "*.[c|S]")
SRCS 			+= $(shell find -L ./board/ -name "*.c")
INC_PATH  := include/ kernel/framework/ kernel/include kernel/klib/include
INC_PATH  += board/k210/include

PWD := $(shell pwd)

# export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := riscv64-mycpu
# export ARCH    := x86_64-qemu
endif
AM_HOME = $(PWD)
include AM_Makefile.mk

# image:

mkfs:
	make -C tools mkfs
	./tools/mkfs 64 kernel/build/kernel-riscv64-mycpu kernel/fs-img/
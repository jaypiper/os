NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ ./test/ ./klib/ ./am/ -name "*.[c|S]")
INC_PATH       := include/ framework/ ../include klib/include
PWD := $(shell pwd)

# export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := riscv64-mycpu
# export ARCH    := x86_64-qemu
endif
AM_HOME = $(PWD)/..
include $(AM_HOME)/Makefile

# image:

mkfs:
	make -C tools mkfs
	./tools/mkfs 64 build/kernel-riscv64-mycpu fs-img/
NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ ./test/ -name "*.c")
INC_PATH       := include/ framework/

# export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
endif

include $(AM_HOME)/Makefile
include ../Makefile.lab
image:

mkfs:
	make -C tools mkfs
	./tools/mkfs 64 build/kernel-x86_64-qemu fs-img/
.PHONY: clean mkfs

KERNEL = ../kernel

mkfs:
	gcc -ggdb -O1 -Wall -Werror -o $@ -I$(KERNEL)/framework -I$(KERNEL)/include -I$(AM_HOME)/am/include mkfs.c

mkfs-gdb: mkfs
	gdb --args ./mkfs 64 ../build/kernel-x86_64-qemu ../fs-img

clean:
	rm -rf mkfs

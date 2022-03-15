.PHONY: clean

mkfs: mkfs.c
	gcc -ggdb -O1 -Wall -Werror -o $@ -I../framework -I../include -I$(AM_HOME)/am/include $<

clean:
	rm -rf mkfs

LNK_ADDR = 0x100000000000
# CFLAGS  += -m32 -fno-pic -march=i386 -mstringop-strategy=loop -mno-inline-all-stringops
CFLAGS  += -fcf-protection=none -nostartfiles# remove endbr32 in Ubuntu 20.04 with a CPU newer than Comet Lake
LDFLAGS += -Ttext-segment $(LNK_ADDR)
BUILD_DIR = ./build

LIBS = testlib
NAME ?= forktest
ALL ?= $(NAME) $(LIBS)

SRCS = $(addsuffix .c, $(ALL))

$(NAME): $(SRCS)
	gcc $(CFLAGS) -Wl,$(LDFLAGS) $^ -o $(BUILD_DIR)/$@
	objdump $(BUILD_DIR)/$@ -d > $(BUILD_DIR)/$@.txt

cp2fs: $(NAME)
	cp $(BUILD_DIR)/$(NAME) ../kernel/fs-img
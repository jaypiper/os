#include <common.h>
#include <vfs.h>

// null 读写，读永远返回 EOF，写数据直接成功 (丢弃)；

int null_read(ofile_info_t* ofile, int fd, void *buf, int count){
  return 0;
}

int null_write(ofile_info_t* ofile, int fd, void* buf, int count){
  return count;
}

int null_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
  return 0;
}

// zero 只读，永远返回 0 的序列

int zero_read(ofile_info_t* ofile, int fd, void *buf, int count){
  memset(buf, 0, count);
  return count;
}

int zero_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
  return 0;
}

// random 只读，返回随机的字节序列

int random_read(ofile_info_t* ofile, int fd, void *buf, int count){
  for(int i = 0; i < count; i++) ((char*)buf)[i] = rand() & 0xff;
  return count;
}

int random_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
  return 0;
}

// events readonly

int events_read(ofile_info_t* ofile, int fd, void *buf, int count){
  char cmd[128];
  device_t *tty = dev->lookup("tty1");
  int nread = tty->ops->read(tty, 0, buf, sizeof(cmd) - 1);
  return nread;
}

int useless_lseek(ofile_info_t* ofile, int fd, int offset, int whence){
  return 0;
}

#include <vfs.h>

int null_read(ofile_info_t* ofile, int fd, void *buf, int count);
int null_write(ofile_info_t* ofile, int fd, void* buf, int count);
int null_lseek(ofile_info_t* ofile, int fd, int offset, int whence);

int zero_read(ofile_info_t* ofile, int fd, void *buf, int count);
int zero_lseek(ofile_info_t* ofile, int fd, int offset, int whence);

int random_read(ofile_info_t* ofile, int fd, void *buf, int count);
int random_lseek(ofile_info_t* ofile, int fd, int offset, int whence);

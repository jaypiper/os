#include <common.h>
#include <vfs.h>
#include <user.h>

/* based on xv6 usertest.c */


char buf[BLK_SIZE * 16];

void bigfile(char *s){
  int N = 20, SZ=600;
  int fd, i, total, cc;

  // unlink("bigfile.dat");
  fd = vfs->open("bigfile.dat", O_CREAT | O_RDWR);
  if(fd < 0){
    printf("%s: cannot create bigfile\n", s);
    return;
  }
  for(i = 0; i < N; i++){
    memset(buf, i, SZ);
    if(vfs->write(fd, buf, SZ) != SZ){
      printf("%s: write bigfile failed\n", s);
      return;
    }
  }

  struct ufs_stat stat;
  vfs->fstat(fd, &stat);
  printf("after write, id=%d type=%d size=%d\n", stat.id, stat.type, stat.size);

  vfs->close(fd);

  fd = vfs->open("bigfile.dat", O_RDONLY);
  if(fd < 0){
    printf("%s: cannot open bigfile\n", s);
    return;
  }
  total = 0;
  for(i = 0; ; i++){
    cc = vfs->read(fd, buf, SZ/2);
    if(cc < 0){
      printf("%s: read bigfile failed\n", s);
      return;
    }
    if(cc == 0)
      break;
    if(cc != SZ/2){
      printf("%s: short read bigfile\n", s);
      return;
    }
    if(buf[0] != i/2 || buf[SZ/2-1] != i/2){
      printf("%s: read bigfile wrong data. read %d %d expect %d, total %d\n", s, buf[0], buf[SZ/2-1], i/2, total);
      return;
    }
    total += cc;
  }
  vfs->close(fd);
  if(total != N*SZ){
    printf("%s: read bigfile wrong total. read %d total %d\n", s, total, N*SZ);
    return;
  }
}


void dirfile(char *s){
  int fd;

  fd = vfs->open("dirfile", O_CREAT);
  if(fd < 0){
    printf("%s: create dirfile failed\n", s);
    return;
  }
  vfs->close(fd);
  if(vfs->chdir("dirfile") == 0){
    printf("%s: chdir dirfile succeeded!\n", s);
    return;
  }
  fd = vfs->open("dirfile/xx", 0);
  if(fd >= 0){
    printf("%s: create dirfile/xx succeeded!\n", s);
    return;
  }
  fd = vfs->open("dirfile/xx", O_CREAT);
  if(fd >= 0){
    printf("%s: create dirfile/xx succeeded!\n", s);
    return;
  }
  if(vfs->mkdir("dirfile/xx") == 0){
    printf("%s: mkdir dirfile/xx succeeded!\n", s);
    return;
  }
  if(vfs->unlink("dirfile/xx") == 0){
    printf("%s: unlink dirfile/xx succeeded!\n", s);
    return;
  }
  if(vfs->link("README", "dirfile/xx") == 0){
    printf("%s: link to dirfile/xx succeeded!\n", s);
    return;
  }
  if(vfs->unlink("dirfile") != 0){
    printf("%s: unlink dirfile failed!\n", s);
    return;
  }

  vfs->close(fd);
}

void unlinkopen(char *s) {
  int SZ = 5;
  int fd = vfs->open("unlinkopen", O_CREAT | O_RDWR);
  vfs->write(fd, "hello", SZ);
  vfs->close(fd);
  vfs->unlink("unlinkopen");
  if(vfs->open("unlinkopen", O_RDWR) == 0){
    printf("unlinkopen fail1\n");
    return;
  }
  fd = vfs->open("unlinkopen", O_CREAT | O_RDWR);
  vfs->write(fd, "aaaaa", SZ);
  vfs->close(fd);
  fd = vfs->open("unlinkopen2", O_CREAT | O_RDWR);
  vfs->write(fd, "bbbbb", SZ);
  vfs->close(fd);
  vfs->unlink("unlinkopen");
  if(vfs->open("unlinkopen", O_RDWR) == 0){
    printf("unlinkopen fail2\n");
    return;
  }
  fd = vfs->open("unlinkopen2", O_RDWR);
  if(fd < 0){
    printf("unlinkopen fail3\n");
    return;
  }
  vfs->read(fd, buf, SZ);
  if(buf[0] != 'b'){
    printf("unlinkopen fail4\n");
    return;
  }
}


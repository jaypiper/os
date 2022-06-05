#include <common.h>
#include <fat32.h>
#include <user.h>

/* based on xv6 usertest.c */
#ifdef VFS_DEBUG
#define BLK_SIZE 512

int bigfile(char *s){
  char buf[BLK_SIZE];

  int N = 20, SZ=600;
  int fd, i, total, cc;

  // unlink("bigfile.dat");
  fd = vfs->openat(AT_FDCWD, "bigfile.dat", O_CREAT | O_RDWR);
  if(fd < 0){
    printf("%s: cannot create bigfile\n", s);
    return -1;
  }
  for(i = 0; i < N; i++){
    memset(buf, i, SZ);
    if(vfs->write(fd, buf, SZ) != SZ){
      printf("%s: write bigfile failed\n", s);
      return -1;
    }
  }

  struct ufs_stat stat;
  vfs->fstat(fd, &stat);

  vfs->close(fd);

  fd = vfs->openat(AT_FDCWD, "bigfile.dat", O_RDONLY);
  if(fd < 0){
    printf("%s: cannot open bigfile\n", s);
    return -1;
  }
  total = 0;
  for(i = 0; ; i++){
    cc = vfs->read(fd, buf, SZ/2);
    if(cc < 0){
      printf("%s: read bigfile failed\n", s);
      return -1;
    }
    if(cc == 0)
      break;
    if(cc != SZ/2){
      printf("%s: short read bigfile\n", s);
      return -1;
    }
    if(buf[0] != i/2 || buf[SZ/2-1] != i/2){
      printf("%s: read bigfile wrong data. read %d %d expect %d, total %d\n", s, buf[0], buf[SZ/2-1], i/2, total);
      return -1;
    }
    total += cc;
  }
  vfs->close(fd);
  if(total != N*SZ){
    printf("%s: read bigfile wrong total. read %d total %d\n", s, total, N*SZ);
    return -1;
  }
  return 0;
}

// directory that uses indirect blocks
int bigdir(char *s) {
  enum { N = 400 };
  int i, fd;
  char name[10];

  vfs->unlink("bd");

  fd = vfs->openat(AT_FDCWD, "bd", O_CREAT);
  if(fd < 0){
    printf("%s: bigdir create failed\n", s);
    return -1;
  }
  vfs->close(fd);

  for(i = 0; i < N; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(vfs->link("bd", name) != 0){
      printf("%s: bigdir link(bd, %s) failed, i=%d\n", s, name, i);
      return -1;
    }
  }

  vfs->unlink("bd");
  for(i = 0; i < N; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(vfs->unlink(name) != 0){
      printf("%s: bigdir unlink failed", s);
      return -1;
    }
  }
  return 0;
}

int dirfile(char *s){
  int fd;

  fd = vfs->openat(AT_FDCWD, "dirfile", O_CREAT);
  if(fd < 0){
    printf("%s: create dirfile failed\n", s);
    return -1;
  }
  vfs->close(fd);
  if(vfs->chdir("dirfile") == 0){
    printf("%s: chdir dirfile succeeded!\n", s);
    return -1;
  }
  fd = vfs->openat(AT_FDCWD, "dirfile/xx", 0);
  if(fd >= 0){
    printf("%s: create dirfile/xx succeeded!\n", s);
    return -1;
  }
  fd = vfs->openat(AT_FDCWD, "dirfile/xx", O_CREAT);
  if(fd >= 0){
    printf("%s: create dirfile/xx succeeded!\n", s);
    return -1;
  }
  if(vfs->mkdir("dirfile/xx") == 0){
    printf("%s: mkdir dirfile/xx succeeded!\n", s);
    return -1;
  }
  if(vfs->unlink("dirfile/xx") == 0){
    printf("%s: unlink dirfile/xx succeeded!\n", s);
    return -1;
  }
  if(vfs->link("README", "dirfile/xx") == 0){
    printf("%s: link to dirfile/xx succeeded!\n", s);
    return -1;
  }
  if(vfs->unlink("dirfile") != 0){
    printf("%s: unlink dirfile failed!\n", s);
    return -1;
  }

  vfs->close(fd);
  return 0;
}

int unlinkopen(char *s) {
  char buf[BLK_SIZE];
  int SZ = 5;
  int fd = vfs->openat(AT_FDCWD, "unlinkopen", O_CREAT | O_RDWR);
  vfs->write(fd, "hello", SZ);
  vfs->close(fd);
  vfs->unlink("unlinkopen");
  if(vfs->openat(AT_FDCWD, "unlinkopen", O_RDWR) >= 0){
    printf("unlinkopen fail1\n");
    return -1;
  }
  fd = vfs->openat(AT_FDCWD, "unlinkopen", O_CREAT | O_RDWR);
  vfs->write(fd, "aaaaa", SZ);
  vfs->close(fd);
  fd = vfs->openat(AT_FDCWD, "unlinkopen2", O_CREAT | O_RDWR);
  vfs->write(fd, "bbbbb", SZ);
  vfs->close(fd);
  vfs->unlink("unlinkopen");
  if(vfs->openat(AT_FDCWD, "unlinkopen", O_RDWR) >= 0){
    printf("unlinkopen fail2\n");
    return -1;
  }
  fd = vfs->openat(AT_FDCWD, "unlinkopen2", O_RDWR);
  if(fd < 0){
    printf("unlinkopen fail3\n");
    return -1;
  }
  vfs->read(fd, buf, SZ);
  if(buf[0] != 'b'){
    printf("unlinkopen fail4\n");
    return -1;
  }
  vfs->close(fd);
  vfs->unlink("unlinkopen2");
  return 0;
}

int linktest(char *s) {
  char buf[BLK_SIZE];
  enum { SZ = 5 };
  int fd;

  vfs->unlink("lf1");
  vfs->unlink("lf2");

  fd = vfs->openat(AT_FDCWD, "lf1", O_CREAT|O_RDWR);
  if(fd < 0){
    printf("%s: create lf1 failed\n", s);
    return -1;
  }
  if(vfs->write(fd, "hello", SZ) != SZ){
    printf("%s: write lf1 failed\n", s);
    return -1;
  }
  vfs->close(fd);

  if(vfs->link("lf1", "lf2") < 0){
    printf("%s: link lf1 lf2 failed\n", s);
    return -1;
  }
  vfs->unlink("lf1");

  if(vfs->openat(AT_FDCWD, "lf1", 0) >= 0){
    printf("%s: unlinked lf1 but it is still there!\n", s);
    return -1;
  }

  fd = vfs->openat(AT_FDCWD, "lf2", 0);
  if(fd < 0){
    printf("%s: open lf2 failed\n", s);
    return -1;
  }
  if(vfs->read(fd, buf, sizeof(buf)) != SZ){
    printf("%s: read lf2 failed\n", s);
    return -1;
  }
  vfs->close(fd);

  if(vfs->link("lf2", "lf2") >= 0){
    printf("%s: link lf2 lf2 succeeded! oops\n", s);
    return -1;
  }

  vfs->unlink("lf2");
  if(vfs->link("lf2", "lf1") >= 0){
    printf("%s: link non-existent succeeded! oops\n", s);
    return -1;
  }

  if(vfs->link(".", "lf1") >= 0){
    printf("%s: link . lf1 succeeded! oops\n", s);
    return -1;
  }

  return 0;
}

int subdir(char *s) {
  char buf[BLK_SIZE];
  int fd, cc;

  vfs->unlink("ff");
  if(vfs->mkdir("dd") != 0){
    printf("%s: mkdir dd failed\n", s);
    return -1;
  }

  fd = vfs->openat(AT_FDCWD, "dd/ff", O_CREAT | O_RDWR);
  if(fd < 0){
    printf("%s: create dd/ff failed\n", s);
    return -1;
  }
  vfs->write(fd, "ff", 2);
  vfs->close(fd);

  if(vfs->unlink("dd") >= 0){
    printf("%s: unlink dd (non-empty dir) succeeded!\n", s);
    return -1;
  }

  if(vfs->mkdir("/dd/dd") != 0){
    printf("subdir mkdir dd/dd failed\n", s);
    return -1;
  }

  fd = vfs->openat(AT_FDCWD, "dd/dd/ff", O_CREAT | O_RDWR);
  if(fd < 0){
    printf("%s: create dd/dd/ff failed\n", s);
    return -1;
  }
  vfs->write(fd, "FF", 2);
  vfs->close(fd);

  fd = vfs->openat(AT_FDCWD, "dd/dd/../ff", 0);
  if(fd < 0){
    printf("%s: open dd/dd/../ff failed\n", s);
    return -1;
  }
  cc = vfs->read(fd, buf, sizeof(buf));
  if(cc != 2 || buf[0] != 'f'){
    printf("%s: dd/dd/../ff wrong content\n", s);
    return -1;
  }
  vfs->close(fd);

  if(vfs->link("dd/dd/ff", "dd/dd/ffff") != 0){
    printf("link dd/dd/ff dd/dd/ffff failed\n", s);
    return -1;
  }

  if(vfs->unlink("dd/dd/ff") != 0){
    printf("%s: unlink dd/dd/ff failed\n", s);
    return -1;
  }
  if(vfs->openat(AT_FDCWD, "dd/dd/ff", O_RDONLY) >= 0){
    printf("%s: open (unlinked) dd/dd/ff succeeded\n", s);
    return -1;
  }

  if(vfs->chdir("dd") != 0){
    printf("%s: chdir dd failed\n", s);
    return -1;
  }
  if(vfs->chdir("dd/../../dd") != 0){
    printf("%s: chdir dd/../../dd failed\n", s);
    return -1;
  }
  if(vfs->chdir("dd/../../../dd") != 0){
    printf("chdir dd/../../dd failed\n", s);
    return -1;
  }
  if(vfs->chdir("./..") != 0){
    printf("%s: chdir ./.. failed\n", s);
    return -1;
  }

  fd = vfs->openat(AT_FDCWD, "dd/dd/ffff", 0);
  if(fd < 0){
    printf("%s: open dd/dd/ffff failed\n", s);
    return -1;
  }
  if(vfs->read(fd, buf, sizeof(buf)) != 2){
    printf("%s: read dd/dd/ffff wrong len\n", s);
    return -1;
  }
  vfs->close(fd);

  if(vfs->openat(AT_FDCWD, "dd/dd/ff", O_RDONLY) >= 0){
    printf("%s: open (unlinked) dd/dd/ff succeeded!\n", s);
    return -1;
  }

  if(vfs->openat(AT_FDCWD, "dd/ff/ff", O_CREAT|O_RDWR) >= 0){
    printf("%s: create dd/ff/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->openat(AT_FDCWD, "dd/xx/ff", O_CREAT|O_RDWR) >= 0){
    printf("%s: create dd/xx/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->openat(AT_FDCWD, "dd", O_RDWR) >= 0){
    printf("%s: open dd rdwr succeeded!\n", s);
    return -1;
  }
  if(vfs->openat(AT_FDCWD, "dd", O_WRONLY) >= 0){
    printf("%s: open dd wronly succeeded!\n", s);
    return -1;
  }
  if(vfs->link("dd/ff/ff", "dd/dd/xx") == 0){
    printf("%s: link dd/ff/ff dd/dd/xx succeeded!\n", s);
    return -1;
  }
  if(vfs->link("dd/xx/ff", "dd/dd/xx") == 0){
    printf("%s: link dd/xx/ff dd/dd/xx succeeded!\n", s);
    return -1;
  }
  if(vfs->link("dd/ff", "dd/dd/ffff") == 0){
    printf("%s: link dd/ff dd/dd/ffff succeeded!\n", s);
    return -1;
  }
  if(vfs->mkdir("dd/ff/ff") == 0){
    printf("%s: mkdir dd/ff/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->mkdir("dd/xx/ff") == 0){
    printf("%s: mkdir dd/xx/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->mkdir("dd/dd/ffff") == 0){
    printf("%s: mkdir dd/dd/ffff succeeded!\n", s);
    return -1;
  }
  if(vfs->unlink("dd/xx/ff") == 0){
    printf("%s: unlink dd/xx/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->unlink("dd/ff/ff") == 0){
    printf("%s: unlink dd/ff/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->chdir("dd/ff") == 0){
    printf("%s: chdir dd/ff succeeded!\n", s);
    return -1;
  }
  if(vfs->chdir("dd/xx") == 0){
    printf("%s: chdir dd/xx succeeded!\n", s);
    return -1;
  }

  if(vfs->unlink("dd/dd/ffff") != 0){
    printf("%s: unlink dd/dd/ff failed\n", s);
    return -1;
  }
  if(vfs->unlink("dd/ff") != 0){
    printf("%s: unlink dd/ff failed\n", s);
    return -1;
  }
  if(vfs->unlink("dd") == 0){
    printf("%s: unlink non-empty dd succeeded!\n", s);
    return -1;
  }
  if(vfs->unlink("dd/dd") < 0){
    printf("%s: unlink dd/dd failed\n", s);
    return -1;
  }
  if(vfs->unlink("dd") < 0){
    printf("%s: unlink dd failed\n", s);
    return -1;
  }

  return 0;
}

int rmdot(char *s) {
  if(vfs->mkdir("dots") != 0){
    printf("%s: mkdir dots failed\n", s);
    return -1;
  }
  if(vfs->chdir("dots") != 0){
    printf("%s: chdir dots failed\n", s);
    return -1;
  }
  if(vfs->unlink(".") == 0){
    printf("%s: rm . worked!\n", s);
    return -1;
  }
  if(vfs->unlink("..") == 0){
    printf("%s: rm .. worked!\n", s);
    return -1;
  }
  if(vfs->chdir("/") != 0){
    printf("%s: chdir / failed\n", s);
    return -1;
  }
  if(vfs->unlink("dots/.") == 0){
    printf("%s: unlink dots/. worked!\n", s);
    return -1;
  }
  if(vfs->unlink("dots/..") == 0){
    printf("%s: unlink dots/.. worked!\n", s);
    return -1;
  }
  if(vfs->unlink("dots") != 0){
    printf("%s: unlink dots failed!\n", s);
    return -1;
  }

  return 0;
}

// recursive mkdir
// also tests empty file names.
int iref(char *s) {
  int N = 50;
  int i, fd;

  for(i = 0; i < N; i++){
    if(vfs->mkdir("irefd") != 0){
      printf("%s: mkdir irefd failed %d\n", s, i);
      return -1;
    }
    if(vfs->chdir("irefd") != 0){
      printf("%s: chdir irefd failed\n", s);
      return -1;
    }

    vfs->mkdir("");
    vfs->link("README", "");
    fd = vfs->openat(AT_FDCWD, "", O_CREAT);
    if(fd >= 0)
      vfs->close(fd);
    fd = vfs->openat(AT_FDCWD, "xx", O_CREAT);
    if(fd >= 0)
      vfs->close(fd);
    vfs->unlink("xx");
  }

  // clean up
  for(i = 0; i < N; i++){
    vfs->chdir("..");
    vfs->unlink("irefd");
  }

  vfs->chdir("/");

  return 0;
}

int duptest(char* s){
  char buf[BLK_SIZE];
  int fd = vfs->openat(AT_FDCWD, "file", O_CREAT | O_RDWR);
  int fd2 = vfs->dup(fd);
  if(fd2 < 0){
    printf("%s: dup fail\n", s);
    return -1;
  }
  vfs->write(fd, "hello", 5);
  vfs->lseek(fd2, 0, SEEK_SET);
  vfs->read(fd, buf, 5);
  if(strncmp(buf, "hello", 5) != 0){
    printf("%s: dup read fd fail. read %s\n", s, buf);
    return -1;
  }
  vfs->write(fd2, "aaa", 3);
  vfs->close(fd2);
  vfs->lseek(fd, 0, SEEK_SET);
  vfs->read(fd, buf, 8);
  if(strncmp(buf, "helloaaa", 8) != 0){
    printf("%s: dup read fd fail2. read %s\n", s, buf);
    return -1;
  }
  if(vfs->read(fd2, buf, 1) > 0){
    printf("%s, dup read sth at the end of file. %d\n", buf[0]);
    return -1;
  }
  vfs->close(fd);
  return 0;
}

int devtest(char* s){
  char buf[BLK_SIZE];
  int SZ = 12;
  // zero
  int fd = vfs->openat(AT_FDCWD, "/dev/zero", O_RDONLY);
  if(fd < 0){
    printf("%s: open zero fail\n", s);
    return -1;
  }
  memset(buf, 'a', SZ);
  if(vfs->read(fd, buf, SZ) != SZ){
    printf("%s: read zero size\n", s);
    return -1;
  }
  for(int i = 0; i < SZ; i++){
    if(buf[i] != 0){
      printf("%s: read non-zero from /dev/zero\n", s);
      return -1;
    }
  }
  if(vfs->close(fd) != 0){
    printf("%s: dev close zero\n", s);
    return -1;
  }
  // random
  fd = vfs->openat(AT_FDCWD, "/dev/random", O_RDONLY);
  if(fd < 0){
    printf("%s: open random fail\n", s);
    return -1;
  }
  if(vfs->read(fd, buf, SZ) != SZ){
    printf("%s: read random size\n", s);
    return -1;
  }
  if(vfs->close(fd) != 0){
    printf("%s: dev close random\n", s);
    return -1;
  }
  // null
  fd = vfs->openat(AT_FDCWD, "/dev/null", O_RDWR);
  if(fd < 0){
    printf("%s: open null fail\n", s);
    return -1;
  }
  int wdsize = vfs->write(fd, buf, SZ);
  if(wdsize != SZ){
    printf("%s: write size %d\n", s, wdsize);
    return -1;
  }
  if(vfs->close(fd) != 0){
    printf("%s: dev close null\n", s);
    return -1;
  }
  // TODO: sda test
  return 0;
}

int proctest(char* s){
  char buf[BLK_SIZE];
  int SZ = 20;
  // cpuinfo
  memset(buf, 0, SZ);
  int fd = vfs->openat(AT_FDCWD, "/proc/cpuinfo", O_RDONLY);
  if(fd < 0){
    printf("%s: open cpuinfo fail\n", s);
    return -1;
  }
  vfs->read(fd, buf, SZ);
  vfs->close(fd);
  // meminfo
  memset(buf, 0, SZ);
  fd = vfs->openat(AT_FDCWD, "/proc/meminfo", O_RDONLY);
  if(fd < 0){
    printf("%s: open meminfo fail\n", s);
    return -1;
  }
  vfs->read(fd, buf, SZ);
  vfs->close(fd);
  // proc 1
  memset(buf, 0, SZ);
  vfs->openat(AT_FDCWD, "/proc/1/name", O_RDONLY);
  vfs->read(fd, buf, SZ);
  vfs->close(fd);
  return 0;
}

void filetest(int idx){
  struct test {
    int (*f)(char *);
    char *s;
  } tests[] = {
    {bigfile, "bigfile"},
    {bigdir, "bigdir"},
    {dirfile, "dirfile"},
    {unlinkopen, "unlinkopen"},
    {linktest, "linktest"},
    {subdir, "subdir"},
    {rmdot, "rmdot"},
    {iref, "iref"},
    {duptest, "duptest"},
    {devtest, "devtest"},
    {proctest, "proctest"},
    {0, 0}
  };

  if(idx < 0){
    for (struct test *t = tests; t->s != 0; t++){
      if(t->f(t->s) == -1) return;
    }
    printf("all file tests passed!!\n");
    return;
  } else if(idx < (sizeof(tests) / sizeof(tests[0]) - 1)){
    struct test *t = &tests[idx];
    while(1){
      if(t->f(t->s) == -1){
        Assert(0, "idx %d %s fail\n", idx, t->s);
        while(1);
      }
    }
  } else{
    while(1) ;
  }
}
#endif
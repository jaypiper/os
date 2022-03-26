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

// directory that uses indirect blocks
void bigdir(char *s) {
  enum { N = 500 };
  int i, fd;
  char name[10];

  vfs->unlink("bd");

  fd = vfs->open("bd", O_CREAT);
  if(fd < 0){
    printf("%s: bigdir create failed\n", s);
    return;
  }
  vfs->close(fd);

  for(i = 0; i < N; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(vfs->link("bd", name) != 0){
      printf("%s: bigdir link(bd, %s) failed, i=%d\n", s, name, i);
      return;
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
      return;
    }
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

void linktest(char *s) {
  enum { SZ = 5 };
  int fd;

  vfs->unlink("lf1");
  vfs->unlink("lf2");

  fd = vfs->open("lf1", O_CREAT|O_RDWR);
  if(fd < 0){
    printf("%s: create lf1 failed\n", s);
    return;
  }
  if(vfs->write(fd, "hello", SZ) != SZ){
    printf("%s: write lf1 failed\n", s);
    return;
  }
  vfs->close(fd);

  if(vfs->link("lf1", "lf2") < 0){
    printf("%s: link lf1 lf2 failed\n", s);
    return;
  }
  vfs->unlink("lf1");

  if(vfs->open("lf1", 0) >= 0){
    printf("%s: unlinked lf1 but it is still there!\n", s);
    return;
  }

  fd = vfs->open("lf2", 0);
  if(fd < 0){
    printf("%s: open lf2 failed\n", s);
    return;
  }
  if(vfs->read(fd, buf, sizeof(buf)) != SZ){
    printf("%s: read lf2 failed\n", s);
    return;
  }
  vfs->close(fd);

  if(vfs->link("lf2", "lf2") >= 0){
    printf("%s: link lf2 lf2 succeeded! oops\n", s);
    return;
  }

  vfs->unlink("lf2");
  if(vfs->link("lf2", "lf1") >= 0){
    printf("%s: link non-existent succeeded! oops\n", s);
    return;
  }

  if(vfs->link(".", "lf1") >= 0){
    printf("%s: link . lf1 succeeded! oops\n", s);
    return;
  }
}

void subdir(char *s) {
  int fd, cc;

  vfs->unlink("ff");
  if(vfs->mkdir("dd") != 0){
    printf("%s: mkdir dd failed\n", s);
    return;
  }

  fd = vfs->open("dd/ff", O_CREAT | O_RDWR);
  if(fd < 0){
    printf("%s: create dd/ff failed\n", s);
    return;
  }
  vfs->write(fd, "ff", 2);
  vfs->close(fd);

  if(vfs->unlink("dd") >= 0){
    printf("%s: unlink dd (non-empty dir) succeeded!\n", s);
    return;
  }

  if(vfs->mkdir("/dd/dd") != 0){
    printf("subdir mkdir dd/dd failed\n", s);
    return;
  }

  fd = vfs->open("dd/dd/ff", O_CREAT | O_RDWR);
  if(fd < 0){
    printf("%s: create dd/dd/ff failed\n", s);
    return;
  }
  vfs->write(fd, "FF", 2);
  vfs->close(fd);

  fd = vfs->open("dd/dd/../ff", 0);
  if(fd < 0){
    printf("%s: open dd/dd/../ff failed\n", s);
    return;
  }
  cc = vfs->read(fd, buf, sizeof(buf));
  if(cc != 2 || buf[0] != 'f'){
    printf("%s: dd/dd/../ff wrong content\n", s);
    return;
  }
  vfs->close(fd);

  if(vfs->link("dd/dd/ff", "dd/dd/ffff") != 0){
    printf("link dd/dd/ff dd/dd/ffff failed\n", s);
    return;
  }

  if(vfs->unlink("dd/dd/ff") != 0){
    printf("%s: unlink dd/dd/ff failed\n", s);
    return;
  }
  if(vfs->open("dd/dd/ff", O_RDONLY) >= 0){
    printf("%s: open (unlinked) dd/dd/ff succeeded\n", s);
    return;
  }

  if(vfs->chdir("dd") != 0){
    printf("%s: chdir dd failed\n", s);
    return;
  }
  if(vfs->chdir("dd/../../dd") != 0){
    printf("%s: chdir dd/../../dd failed\n", s);
    return;
  }
  if(vfs->chdir("dd/../../../dd") != 0){
    printf("chdir dd/../../dd failed\n", s);
    return;
  }
  if(vfs->chdir("./..") != 0){
    printf("%s: chdir ./.. failed\n", s);
    return;
  }

  fd = vfs->open("dd/dd/ffff", 0);
  if(fd < 0){
    printf("%s: open dd/dd/ffff failed\n", s);
    return;
  }
  if(vfs->read(fd, buf, sizeof(buf)) != 2){
    printf("%s: read dd/dd/ffff wrong len\n", s);
    return;
  }
  vfs->close(fd);

  if(vfs->open("dd/dd/ff", O_RDONLY) >= 0){
    printf("%s: open (unlinked) dd/dd/ff succeeded!\n", s);
    return;
  }

  if(vfs->open("dd/ff/ff", O_CREAT|O_RDWR) >= 0){
    printf("%s: create dd/ff/ff succeeded!\n", s);
    return;
  }
  if(vfs->open("dd/xx/ff", O_CREAT|O_RDWR) >= 0){
    printf("%s: create dd/xx/ff succeeded!\n", s);
    return;
  }
  if(vfs->open("dd", O_RDWR) >= 0){
    printf("%s: open dd rdwr succeeded!\n", s);
    return;
  }
  if(vfs->open("dd", O_WRONLY) >= 0){
    printf("%s: open dd wronly succeeded!\n", s);
    return;
  }
  if(vfs->link("dd/ff/ff", "dd/dd/xx") == 0){
    printf("%s: link dd/ff/ff dd/dd/xx succeeded!\n", s);
    return;
  }
  if(vfs->link("dd/xx/ff", "dd/dd/xx") == 0){
    printf("%s: link dd/xx/ff dd/dd/xx succeeded!\n", s);
    return;
  }
  if(vfs->link("dd/ff", "dd/dd/ffff") == 0){
    printf("%s: link dd/ff dd/dd/ffff succeeded!\n", s);
    return;
  }
  if(vfs->mkdir("dd/ff/ff") == 0){
    printf("%s: mkdir dd/ff/ff succeeded!\n", s);
    return;
  }
  if(vfs->mkdir("dd/xx/ff") == 0){
    printf("%s: mkdir dd/xx/ff succeeded!\n", s);
    return;
  }
  if(vfs->mkdir("dd/dd/ffff") == 0){
    printf("%s: mkdir dd/dd/ffff succeeded!\n", s);
    return;
  }
  if(vfs->unlink("dd/xx/ff") == 0){
    printf("%s: unlink dd/xx/ff succeeded!\n", s);
    return;
  }
  if(vfs->unlink("dd/ff/ff") == 0){
    printf("%s: unlink dd/ff/ff succeeded!\n", s);
    return;
  }
  if(vfs->chdir("dd/ff") == 0){
    printf("%s: chdir dd/ff succeeded!\n", s);
    return;
  }
  if(vfs->chdir("dd/xx") == 0){
    printf("%s: chdir dd/xx succeeded!\n", s);
    return;
  }

  if(vfs->unlink("dd/dd/ffff") != 0){
    printf("%s: unlink dd/dd/ff failed\n", s);
    return;
  }
  if(vfs->unlink("dd/ff") != 0){
    printf("%s: unlink dd/ff failed\n", s);
    return;
  }
  if(vfs->unlink("dd") == 0){
    printf("%s: unlink non-empty dd succeeded!\n", s);
    return;
  }
  if(vfs->unlink("dd/dd") < 0){
    printf("%s: unlink dd/dd failed\n", s);
    return;
  }
  if(vfs->unlink("dd") < 0){
    printf("%s: unlink dd failed\n", s);
    return;
  }
}

void rmdot(char *s) {
  if(vfs->mkdir("dots") != 0){
    printf("%s: mkdir dots failed\n", s);
    return;
  }
  if(vfs->chdir("dots") != 0){
    printf("%s: chdir dots failed\n", s);
    return;
  }
  if(vfs->unlink(".") == 0){
    printf("%s: rm . worked!\n", s);
    return;
  }
  if(vfs->unlink("..") == 0){
    printf("%s: rm .. worked!\n", s);
    return;
  }
  if(vfs->chdir("/") != 0){
    printf("%s: chdir / failed\n", s);
    return;
  }
  if(vfs->unlink("dots/.") == 0){
    printf("%s: unlink dots/. worked!\n", s);
    return;
  }
  if(vfs->unlink("dots/..") == 0){
    printf("%s: unlink dots/.. worked!\n", s);
    return;
  }
  if(vfs->unlink("dots") != 0){
    printf("%s: unlink dots failed!\n", s);
    return;
  }
}

// recursive mkdir
// also tests empty file names.
void iref(char *s) {
  int N = 50;
  int i, fd;

  for(i = 0; i < N; i++){
    if(vfs->mkdir("irefd") != 0){
      printf("%s: mkdir irefd failed %d\n", s, i);
      return;
    }
    if(vfs->chdir("irefd") != 0){
      printf("%s: chdir irefd failed\n", s);
      return;
    }

    vfs->mkdir("");
    vfs->link("README", "");
    fd = vfs->open("", O_CREAT);
    if(fd >= 0)
      vfs->close(fd);
    fd = vfs->open("xx", O_CREAT);
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
}

void duptest(char* s){
  int fd = vfs->open("file", O_CREAT | O_RDWR);
  int fd2 = vfs->dup(fd);
  vfs->write(fd, "hello", 5);
  vfs->lseek(fd2, 0, SEEK_SET);
  vfs->read(fd, buf, 5);
  printf("buf=%s\n", buf);
  vfs->write(fd2, "aaa", 3);
  vfs->close(fd2);
  vfs->lseek(fd, 0, SEEK_SET);
  vfs->read(fd, buf, 8);
  printf("buf=%s\n", buf);
  vfs->read(fd2, buf, 1);
}
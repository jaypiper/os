#include <am.h>

#define BLKSZ  512
#define DISKSZ (64 << 20)
#define DISK_START 0x40000000

void disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->blksz   = BLKSZ;
  cfg->blkcnt  = DISKSZ / BLKSZ;
}

void disk_status(AM_DISK_STATUS_T *status) {
  status->ready = true;
}

void disk_blkio(AM_DISK_BLKIO_T *bio) {
  uint32_t blkno = bio->blkno, remain = bio->blkcnt;
  uint64_t *ptr = bio->buf;
  uint64_t *db = (uint64_t *)DISK_START;
  for (remain = bio->blkcnt; remain; remain--, blkno++) {
    int start = blkno * BLKSZ / 8;
    if (bio->write) {
      for (int i = 0; i < BLKSZ / 8; i ++){
        db[start + i] = *ptr++;
      }
    } else {
      for (int i = 0; i < BLKSZ / 8; i ++){
        *ptr++ = db[start + i];
      }
    }
  }
}

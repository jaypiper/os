#include <am.h>
#include <sdcard.h>

#define BLKSZ  512
#define DISKSZ (64 << 20)
#define DISK_START (uintptr_t)0x84200000


void disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->blksz   = BLKSZ;
  cfg->blkcnt  = DISKSZ / BLKSZ;
}

void disk_status(AM_DISK_STATUS_T *status) {
  status->ready = true;
}

void disk_blkio(AM_DISK_BLKIO_T *bio) {
  uint32_t blkno = bio->blkno, blkcnt = bio->blkcnt;
  uint8_t *ptr = bio->buf;
  while(blkcnt --){
    if (bio->write) {
      memcpy(DISK_START + blkno * BLKSZ, ptr, BLKSZ);
      // sdcard_write_sector(ptr, blkno);
    } else{
      // sdcard_read_sector(ptr, blkno);
      memcpy(ptr, DISK_START + blkno * BLKSZ, BLKSZ);
    }
    ptr += BLKSZ;
  }

}

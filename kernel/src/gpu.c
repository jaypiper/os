#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <riscv64.h>
#include <mycpu.h>

#define VGACTL_ADDR  0xa1000100
#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {

}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  uint32_t _wh = inl(VGACTL_ADDR);
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = _wh >> 16, .height = _wh & 0xffff,
    .vmemsz = 0
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  AM_GPU_CONFIG_T _config = io_read(AM_GPU_CONFIG);
  int w = _config.width;  
  int h = _config.height;  
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  for(int i = 0; i < ctl->h; i++){
    for(int j = 0; j < ctl->w; j++){
      if((i + ctl->y < h) && (j + ctl->x < w)){
        fb[w*(ctl->y+i)+ctl->x+j] = *((uint32_t*)ctl->pixels+i*ctl->w+j);
      }

    }
  }
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}

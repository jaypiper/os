#ifndef OS_UPROC_H
#define OS_UPROC_H

#define MAX_MMAP_NUM 16

typedef struct mm_area {
  uintptr_t start, end;
  int fd, prot, flags;
  size_t offset;
}mm_area_t;

struct page {
  int flags;   // 页的状态，如copy-on-write标记
  int refcnt;  // 引用计数
  void *paddr; // 物理地址
};

void *pgalloc(int size);
void pgfree(void *ptr);

#endif

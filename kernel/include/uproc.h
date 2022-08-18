#ifndef OS_UPROC_H
#define OS_UPROC_H

#include <clone.h>

#define MAX_MMAP_NUM 64

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

typedef struct {
  uintptr_t a_type;
  union{
    uintptr_t a_val;
    void* a_ptr;
    void (*a_fcn)();
  }a_un;
}auxv_t;

#define ADD_AUX(ptr, type, val) \
  ptr -= 2 * sizeof(uintptr_t); \
  aux.a_type = type; aux.a_un.a_val = (uintptr_t)val; \
  memcpy(ptr, &aux, sizeof(auxv_t));

#define AT_NULL 0    /* End of vector */
#define AT_PHDR 3    /* Program headers for program */
#define AT_PHENT 4   /* Size of program header entry */
#define AT_PHNUM 5   /* Number of program headers */
#define AT_PAGESZ 6  /* System page size */
#define AT_BASE 7    /* Base address of interpreter */
#define AT_FLAGS 8   /* Flags */
#define AT_ENTRY 9   /* Entry point of program */

#endif

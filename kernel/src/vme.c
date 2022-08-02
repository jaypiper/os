#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <riscv64.h>
#include <common.h>

const struct mmu_config mmu = {
  .pgsize = 4096,
  .ptlevels = 3,
  .pgtables = {
    // { "SATP", 0x0000000000,  0,  0 },
    { "M1",   0x7fc0000000, 30,  9 },
    { "M2",   0x003fe00000, 21,  9 },
    { "M3",   0x00001ff000, 12,  9 },
  },
};

extern char _addr_start;

// k210 peripherals
// (0x0200_0000, 0x1000),      /* CLINT     */
// (0x0C20_0000, 0x1000),      /* PLIC      */
// (0x3800_0000, 0x1000),      /* UARTHS    */
// (0x3800_1000, 0x1000),      /* GPIOHS    */
// (0x5020_0000, 0x1000),      /* GPIO      */
// (0x5024_0000, 0x1000),      /* SPI_SLAVE */
// (0x502B_0000, 0x1000),      /* FPIOA     */
// (0x502D_0000, 0x1000),      /* TIMER0    */
// (0x502E_0000, 0x1000),      /* TIMER1    */
// (0x502F_0000, 0x1000),      /* TIMER2    */
// (0x5044_0000, 0x1000),      /* SYSCTL    */
// (0x5200_0000, 0x1000),      /* SPI0      */
// (0x5300_0000, 0x1000),      /* SPI1      */
// (0x5400_0000, 0x1000),      /* SPI2      */
// (0x8000_0000, 0x600000),    /* Memory    */

static const struct vm_area vm_areas[] = {
  // { RANGE(0x2000000000, 0x4000000000), 0 },
  { RANGE(0x0, 0x01000000), 0 },
  // { RANGE(0x0000000000, 0x100000000), 1 },
  {RANGE((uintptr_t)0x10000000, (uintptr_t)0x10000000 + 0x1000), 1}, //UART
  {RANGE((uintptr_t)0x80000000, (uintptr_t)0x80000000 + 6 * 1024 * 1024), 1},
  {RANGE((uintptr_t)0x84000000, (uintptr_t)0x84000000 + 4 * 1024 * 1024), 1}
  // {RANGE((uintptr_t)0x02000000, (uintptr_t)0x02000000 + 0x1000), 1},  // CLINT
  // {RANGE((uintptr_t)0x0C200000, (uintptr_t)0x0C200000 + 0x1000), 1},  // PLIC
  // {RANGE((uintptr_t)0x38000000, (uintptr_t)0x38001000), 1},           // UARTHS
  // {RANGE((uintptr_t)0x38001000, (uintptr_t)0x38001000 + 0x1000), 1},  // GPIOHS
  // {RANGE((uintptr_t)0x50000000, (uintptr_t)0x50000000 + 0x1000), 1},  // DMAC
  // {RANGE((uintptr_t)0x50200000, (uintptr_t)0x50200000 + 0x1000), 1},  // GPIO
  // {RANGE((uintptr_t)0x50240000, (uintptr_t)0x50240000 + 0x1000), 1},  // SPI-slave
  // {RANGE((uintptr_t)0x502B0000, (uintptr_t)0x502B0000 + 0x1000), 1},  // FPIOA
  // {RANGE((uintptr_t)0x502D0000, (uintptr_t)0x502D0000 + 0x1000), 1},  // TIMER0
  // {RANGE((uintptr_t)0x502E0000, (uintptr_t)0x502E0000 + 0x1000), 1},  // TIMER1
  // {RANGE((uintptr_t)0x502F0000, (uintptr_t)0x502F0000 + 0x1000), 1},  // TIMER2
  // {RANGE((uintptr_t)0x50440000, (uintptr_t)0x50440000 + 0x1000), 1},  // SYSCTL
  // {RANGE((uintptr_t)0x52000000, (uintptr_t)0x52000000 + 0x1000), 1},  // SPI0
  // {RANGE((uintptr_t)0x53000000, (uintptr_t)0x53000000 + 0x1000), 1},  // SPI1
  // {RANGE((uintptr_t)0x54000000, (uintptr_t)0x54000000 + 0x1000), 1},  // SPI2

};

#define uvm_area (vm_areas[0].area)

#define SATP_MASK 0xfffffffffffLL

static uintptr_t *kpt;
static void *(*pgalloc)(int size);
static void (*pgfree)(void *);

static void *pgallocz() {
  uintptr_t *base = pgalloc(mmu.pgsize);
  panic_on(!base, "cannot allocate page");
  for (int i = 0; i < mmu.pgsize / sizeof(uintptr_t); i++) {
    base[i] = 0;
  }
  return base;
}

static int indexof(uintptr_t addr, const struct ptinfo *info) {
  return ((uintptr_t)addr & info->mask) >> info->shift;
}

static uintptr_t baseof(uintptr_t pte) {
  return (pte & ~(0x3ff)) << 2;
}

// static uintptr_t pgstart(uintptr_t addr){
//   return addr & ~(mmu.pgsize - 1);
// }

static uintptr_t flagsof(uintptr_t pte){
  return pte & 0x3ff;
}

static uintptr_t *ptwalk(AddrSpace *as, uintptr_t addr, int flags) {
  uintptr_t cur = (uintptr_t)as->ptr;

  for (int i = 0; i < mmu.ptlevels; i++) {
    const struct ptinfo *ptinfo = &mmu.pgtables[i];
    uintptr_t *pt = (uintptr_t *)cur, next_page;
    int index = indexof(addr, ptinfo);
    if (i == mmu.ptlevels - 1) return &pt[index];

    if (!(pt[index] & PTE_V)) {
      next_page = (uintptr_t)pgallocz();
      pt[index] = (next_page >> 2) | PTE_V;// | flags;
    } else {
      next_page = baseof(pt[index]);
    }
    cur = next_page;
  }
  bug();
}

uintptr_t user_addr_translate(uintptr_t satp, uintptr_t user_addr){
  AddrSpace as = {.ptr = (void*)((satp & SATP_MASK) << 12)};
  uintptr_t pte = *ptwalk(&as, user_addr, 0);
  return baseof(pte);
}

static void teardown(int level, uintptr_t *pt) {
  if(level == mmu.ptlevels) return;

  for (int index = 0; index < (1 << mmu.pgtables[level].bits); index++) {
    if ((pt[index] & PTE_V) && ((level != (mmu.ptlevels - 1)) || (pt[index] & PTE_U))) {
      teardown(level + 1, (void *)baseof(pt[index]));
      pgfree((void *)baseof(pt[index]));
    }
  }
  if (level == 0) pgfree(pt);
}

static inline void sfence_vma() {
  // the zero, zero means flush all TLB entries.
  asm volatile("sfence.vma zero, zero");
}

bool vme_init(void *(*_pgalloc)(int size), void (*_pgfree)(void *)) {
  panic_on(cpu_current() != 0, "init VME in non-bootstrap CPU");
  pgalloc = _pgalloc;
  pgfree  = _pgfree;

  AddrSpace as;
  as.ptr = pgallocz();
  for (int i = 0; i < LENGTH(vm_areas); i++) {
    const struct vm_area *vma = &vm_areas[i];
    if (vma->kernel) {
      for (uintptr_t cur = (uintptr_t)vma->area.start;
           cur != (uintptr_t)vma->area.end;
           cur += mmu.pgsize) {
        *ptwalk(&as, cur, PTE_W | PTE_R) = (cur >> 2) | PTE_V | PTE_W | PTE_R | PTE_X; // | PTE_U;
      }
    }
  }
  kpt = as.ptr;

  return true;
}


void init_satp(){
  w_csr("satp", MAKE_SATP(kpt));
  sfence_vma();
}

void protect(AddrSpace *as) {

  as->ptr = pgallocz();
  for (int i = 0; i < LENGTH(vm_areas); i++) {
    const struct vm_area *vma = &vm_areas[i];
    if (vma->kernel) {
      for (uintptr_t cur = (uintptr_t)vma->area.start;
           cur != (uintptr_t)vma->area.end;
           cur += mmu.pgsize) {
        *ptwalk(as, cur, PTE_W | PTE_R) = (cur >> 2) | PTE_V | PTE_W | PTE_R | PTE_X;
      }
    }
  }
  as->pgsize = mmu.pgsize;
  as->area   = uvm_area;
}

void unprotect(AddrSpace *as) {
  teardown(0, (void*)(as->ptr));
}

void map(AddrSpace *as, void *va, void *pa, int prot) {
  panic_on(!IN_RANGE(va, uvm_area), "mapping an invalid address");
  panic_on((uintptr_t)va != ROUNDDOWN(va, mmu.pgsize) ||
           (uintptr_t)pa != ROUNDDOWN(pa, mmu.pgsize), "non-page-boundary address");

  uintptr_t *ptentry = ptwalk(as, (uintptr_t)va, PTE_R | PTE_W | PTE_U);
  if (prot == MMAP_NONE) {
    panic_on(!(*ptentry & PTE_V), "unmapping a non-mapped page");
    *ptentry = 0;
  } else {
    panic_on(*ptentry & PTE_V, "remapping a mapped page");
    uintptr_t pte = ((uintptr_t)pa >> 2) | PTE_V | PTE_U | PTE_R | PTE_X | ((prot & MMAP_WRITE) ? PTE_W : 0);
    *ptentry = pte;
  }
  ptwalk(as, (uintptr_t)va, PTE_R | PTE_W | PTE_U);
}

#include <string.h>

void pgtable_ucopy_level(int level, uintptr_t* oldpt, uintptr_t* newpt, uintptr_t vaddr){
  if(level >= mmu.ptlevels) return;
  for(int idx = 0; idx < (1 << mmu.pgtables[level].bits); idx++){
    if ((oldpt[idx] & PTE_V) && ((level != mmu.ptlevels - 1) || (oldpt[idx] & PTE_U))){
      if(!(newpt[idx] & PTE_V)){
        uintptr_t newpage = (uintptr_t)pgallocz();
        uintptr_t newpte = (newpage >> 2) | flagsof(oldpt[idx]);
        newpt[idx] = newpte;
        // uintptr_t oldpage = baseof(oldpt[idx]);
        if(level == mmu.ptlevels - 1){
          memcpy((void*)baseof(newpt[idx]), (void*)baseof(oldpt[idx]), mmu.pgsize);
        }

      }
      pgtable_ucopy_level(level + 1, (uintptr_t*)baseof(oldpt[idx]), (uintptr_t*)baseof(newpt[idx]), vaddr | ((uintptr_t)idx << mmu.pgtables[level].shift));
    }
  }
}

void pgtable_ucopy(uintptr_t* oldpt, uintptr_t* newpt){
  pgtable_ucopy_level(0, oldpt, newpt, 0);
}

extern void user_trap(void);

Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *ctx = kstack.end - sizeof(Context);
  *ctx = (Context) { 0 };

  ctx->satp = MAKE_SATP(as->ptr);
  ctx->epc = (uintptr_t)entry;
  ctx->gpr[2] = (uintptr_t)uvm_area.end;      // set sp
  r_csr("satp", ctx->kernel_satp);
  ctx->kernel_sp = (uintptr_t)kstack.end - sizeof(Context);
  ctx->kernel_trap = (uintptr_t)user_trap;
  r_csr("sstatus", ctx->status);
  ctx->status &= ~SSTATUS_SPP;
  ctx->status |= SSTATUS_SPIE;

  return ctx;
}

void init_kernel_as(AddrSpace *as){
  as->pgsize = PGSIZE;
  as->area = uvm_area;
  as->ptr = kpt;
}

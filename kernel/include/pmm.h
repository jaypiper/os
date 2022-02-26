#ifndef OS_PMM_H
#define OS_PMM_H

enum{BLOCK_INVALID=1, BLOCK_HEAD, BLOCK_ALLOCATED, BLOCK_FREE, BLOCK_INTERNAL};

#define LCHILD(p) (2 * (p) + 1)
#define RCHILD(p) (2 * (p) + 2)
#define PARENT(p) (((p) - 1) / 2)
#define IS_ROOT(p) ((p) == 0)
#define TREE_LAYER_BEGIN(height) ((1 << (height)) - 1)
#define TREE_LAYER_END(height) ((1 << (height+1)) - 2)
#define MAX_CPU 8
#define SLAB_NUM 9
#define SLAB_MAGIC 0xbeef
#define PG_BITS 12
#define PGMASK 0xfff

#define ALLOC_MAGIC 0xaa
#define MAX_LOG_SIZE 8192
#define FREE_MAGIC 0xbb

#define BITMAP_BITS 64
#define BITMAP_NUM 16
#define BITMAP_FULL(bitmap) ((bitmap) == 0xffffffffffffffffull)
typedef struct slab_metadata{
  uint16_t bit_num;
  uint16_t magic;
  uint16_t free_num;
  uint16_t cpu_id;
  void* next;
  uint64_t bitmap[BITMAP_NUM];
}smeta_t;

typedef struct buddy_entry{
  uint16_t pgnum;
  uint16_t max_pgnum;
  uint16_t type;
  uint16_t height;    // leaf = 0
}bentry_t;

typedef struct slab_t{
  uint32_t total_num;
  smeta_t* page;
}slab_t;

#endif

#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <lock.h>
#include <debug.h>
#include <util.h>
#include <sem.h>
#include <limits.h>
#include <devices.h>

// #define PMM_DEBUG
// #define KMT_DEBUG
// #define VFS_DEBUG
#define UPROC_DEBUG
#define PGSIZE    4096
#define MAX_CPU 8
#define STACK_SIZE (32 * PGSIZE)

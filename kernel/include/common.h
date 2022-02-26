#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <lock.h>
#include <debug.h>

#define PMM_DEBUG
#define PGSIZE    4096

#define MAX(a, b) ((a) > (b))? (a) : (b)
#define MIN(a, b) ((a) > (b))? (b) : (a)

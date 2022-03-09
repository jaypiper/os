#ifndef OS_LOCK_H
#define OS_LOCK_H

typedef struct spinlock{
  int locked;
  const char* name;
  int cpu_id;
}spinlock_t;

void spin_lock(spinlock_t *lk);
void spin_unlock(spinlock_t *lk);
void spin_init(spinlock_t *lk, const char* name);
bool holding(spinlock_t* lk);
void mutex_lock(spinlock_t *lk);
void mutex_unlock(spinlock_t *lk);
int mutex_trylock(spinlock_t* lk);

#define LOCK_FENCE1_MAGIC 0x12345
#define LOCK_FENCE2_MAGIC 0x6789a

#endif

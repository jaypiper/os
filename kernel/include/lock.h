#ifndef OS_LOCK_H
#define OS_LOCK_H

typedef struct spinlock{
  uint32_t fence1;
  int locked;
  const char* name;
  int cpu_id;
  uint32_t fence2;
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
#define CHECK_LOCK(lock) ((lock->fence1 == LOCK_FENCE1_MAGIC) && (lock->fence2 == LOCK_FENCE2_MAGIC))
#define SET_LOCK(lock) do{lock->fence1 = LOCK_FENCE1_MAGIC; lock->fence2 = LOCK_FENCE2_MAGIC;} while(0)

#endif

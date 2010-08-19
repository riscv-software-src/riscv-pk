#ifndef _RISCV_ATOMIC_H
#define _RISCV_ATOMIC_H

typedef struct { long val; } atomic_t;
typedef struct { atomic_t lock; } spinlock_t;
#define SPINLOCK_INIT {{0}}

static inline long atomic_add(atomic_t* a, long inc)
{
  long old = a->val;
  a->val += inc;
  return old;
}

static inline long atomic_swap(atomic_t* a, long val)
{
  long old = a->val;
  a->val = val;
  return old;
}

static inline void atomic_set(atomic_t* a, long val)
{
  a->val = val;
}

static inline long atomic_read(atomic_t* a)
{
  return a->val;
}

static inline void spinlock_lock(spinlock_t* lock)
{
  while(atomic_read(&lock->lock))
    while(atomic_swap(&lock->lock,-1));
}

static inline void spinlock_unlock(spinlock_t* lock)
{
  atomic_set(&lock->lock,0);
}

#endif

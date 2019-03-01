/* ========================================================================= **
**                                      __           __                      **
**                       ______      __/ /___  _____/ /__                    **
**                      / ___/ | /| / / / __ \/ ___/ //_/                    **
**                     / /   | |/ |/ / / /_/ / /__/ ,<                       **
**                    /_/    |__/|__/_/\____/\___/_/|_|                      **
**                                                                           **
** ========================================================================= **
**                      PARTITIONED READER-WRITER LOCK                       **
** ========================================================================= **
**                                                                           **
** Copyright (c) 2002-2018 Jonah H. Harris.                                  **
**                                                                           **
** This library is free software; you can redistribute it and/or modify it   **
** under the terms of the GNU Lesser General Public License as published by  **
** the Free Software Foundation; either version 3 of the License, or (at     **
** your option) any later version.                                           **
**                                                                           **
** This library is distributed in the hope it will be useful, but WITHOUT    **
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     **
** FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public       **
** License for more details.                                                 **
**                                                                           **
** You should have received a copy of the GNU Lesser General Public License  **
** along with this library; if not, write to the Free Software Foundation,   **
** Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                             **
** ========================================================================= */

/* ========================================================================= */
/* -- INCLUSIONS ----------------------------------------------------------- */
/* ========================================================================= */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "prwlock.h"

/* ========================================================================= */
/* -- DEFINITIONS ---------------------------------------------------------- */
/* ========================================================================= */

#define CACHE_LINE_SIZE         64

/* ========================================================================= */
/* -- MACROS --------------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE TYPES -------------------------------------------------------- */
/* ========================================================================= */

#if defined(USE_LIBUV_RWLOCK) || defined(USE_ATOMICS)
typedef enum {
  PRWLOCK_TYPE_NONE,
  PRWLOCK_TYPE_READ,
  PRWLOCK_TYPE_WRITE
} prwlock_type_t;
#endif /* USE_LIBUV_RWLOCK */

typedef struct {
#if defined(USE_LIBUV_RWLOCK)
  uv_rwlock_t                   rwlock;
  prwlock_type_t                lock_type_held;
  char                          cache_line_padding[56
                                  - sizeof(prwlock_type_t)];
#elif defined(USE_ATOMICS) 
  _Atomic int32_t               rwlock;
  prwlock_type_t                lock_type_held;
  char                          cache_line_padding[CACHE_LINE_SIZE
                                  - sizeof(int32_t) - sizeof(prwlock_type_t)];
#else /* pthreads */
  pthread_rwlock_t              rwlock;
  char                          cache_line_padding[56];
#endif /* USE_LIBUV_RWLOCK */
} partitioned_rwlock_cell_t;

struct partitioned_rwlock_t {
  size_t                        partition_count;
  partitioned_rwlock_cell_t    *cells;
};

/* ========================================================================= */
/* -- PRIVATE METHOD PROTOTYPES -------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE DATA --------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PUBLIC DATA ---------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- EXTERNAL DATA -------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- EXTERNAL FUNCTION PROTOTYPES ----------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC ASSERTIONS ---------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE METHODS ------------------------------------------------------ */
/* ========================================================================= */

/* ========================================================================= */
/* -- PUBLIC METHODS ------------------------------------------------------- */
/* ========================================================================= */

int
partitioned_rwlock_init (
  partitioned_rwlock_t        **rwlock,
  size_t                        partition_count
) {
  partitioned_rwlock_t *newlock = NULL;
  if (posix_memalign((void *) &newlock, CACHE_LINE_SIZE, sizeof(*newlock))) {
    printf("Failed to allocate new lock structure!\n");
    return -1;
  }

  newlock->partition_count = partition_count;
  if (posix_memalign((void **) &newlock->cells, CACHE_LINE_SIZE,
    (partition_count * sizeof(*newlock->cells)))) {
    printf("Failed to allocate %zd cells!\n", partition_count);
  }

  for (size_t ii = 0; ii < partition_count; ++ii) {
    int rc = 0;
#if defined(USE_LIBUV_RWLOCK)
    newlock->cells[ii].lock_type_held = PRWLOCK_TYPE_NONE;
    rc = uv_rwlock_init(&(newlock->cells[ii].rwlock));
#elif defined(USE_ATOMICS) 
    newlock->cells[ii].lock_type_held = PRWLOCK_TYPE_NONE;
    newlock->cells[ii].rwlock = 0;
#else
    rc = pthread_rwlock_init(&(newlock->cells[ii].rwlock), NULL);
#endif /* USE_LIBUV_RWLOCK */
    if (0 != rc) {
      printf("init = %d\n", rc);
    }
  }
  *rwlock = newlock;
  return 0;
} /* partitioned_rwlock_init() */

/* ------------------------------------------------------------------------- */

int
partitioned_rwlock_destroy (
  partitioned_rwlock_t         *rwlock
) {
  for (size_t ii = 0; ii < rwlock->partition_count; ++ii) {
#if defined(USE_LIBUV_RWLOCK)
    uv_rwlock_destroy(&(rwlock->cells[ii].rwlock));
#elif defined(USE_ATOMICS) 
    rwlock->cells[ii].rwlock = 0;
#else
    int rc = pthread_rwlock_destroy(&(rwlock->cells[ii].rwlock));
    if (0 != rc) {
      printf("init = %d\n", rc);
    }
#endif /* USE_LIBUV_RWLOCK */
  }
  free(rwlock->cells);
  free(rwlock);
  return 0;
} /* partitioned_rwlock_destroy() */

/* ------------------------------------------------------------------------- */

size_t
partitioned_rwlock_get_partition_count (
  partitioned_rwlock_t         *rwlock
) {
  return rwlock->partition_count;
} /* partitioned_rwlock_get_partition_count() */

/* ------------------------------------------------------------------------- */

int
partitioned_rwlock_rdlock (
  partitioned_rwlock_t         *rwlock,
  const size_t                  partition
) {
  assert(NULL != rwlock);
  assert(partition < rwlock->partition_count);

#if defined(USE_LIBUV_RWLOCK)
  uv_rwlock_rdlock(&(rwlock->cells[partition].rwlock));
  rwlock->cells[partition].lock_type_held = PRWLOCK_TYPE_READ;
  return 0;
#elif defined(USE_ATOMICS) 
  while (1) {
    int32_t val; 
    do {
      val = atomic_load_explicit(&rwlock->cells[partition].rwlock,
        memory_order_relaxed);
      if (0 > val) {
        continue;
      }
    } while (!atomic_compare_exchange_weak_explicit(
      &rwlock->cells[partition].rwlock, &val, (val + 1), memory_order_acquire,
      memory_order_relaxed));
    break;
  }
  rwlock->cells[partition].lock_type_held = PRWLOCK_TYPE_READ;
  return 0;
#else
  return pthread_rwlock_rdlock(&(rwlock->cells[partition].rwlock));
#endif /* USE_LIBUV_RWLOCK */
} /* partitioned_rwlock_rdlock() */

/* ------------------------------------------------------------------------- */

int
partitioned_rwlock_tryrdlock (
  partitioned_rwlock_t         *rwlock,
  const size_t                  partition
) {
  assert(NULL != rwlock);
  assert(partition < rwlock->partition_count);

#if defined(USE_LIBUV_RWLOCK)
  int rc = uv_rwlock_tryrdlock(&(rwlock->cells[partition].rwlock));
  if (0 == rc) {
    rwlock->cells[partition].lock_type_held = PRWLOCK_TYPE_READ;
  }
  return rc;
#elif defined(USE_ATOMICS) 
  int32_t val = atomic_load_explicit(&rwlock->cells[partition].rwlock,
    memory_order_relaxed);
  if (0 > val) {
    return 1;
  } else {
    return partitioned_rwlock_rdlock(rwlock, partition);
  }
#else
  return pthread_rwlock_tryrdlock(&(rwlock->cells[partition].rwlock));
#endif /* USE_LIBUV_RWLOCK */
} /* partitioned_rwlock_tryrdlock() */

/* ------------------------------------------------------------------------- */

int
partitioned_rwlock_trywrlock (
  partitioned_rwlock_t         *rwlock,
  const size_t                  partition
) {
  assert(NULL != rwlock);
  assert(partition < rwlock->partition_count);

#if defined(USE_LIBUV_RWLOCK)
  int rc = uv_rwlock_trywrlock(&(rwlock->cells[partition].rwlock));
  if (0 == rc) {
    rwlock->cells[partition].lock_type_held = PRWLOCK_TYPE_WRITE;
  }
  return rc;
#elif defined(USE_ATOMICS) 
  int32_t val = atomic_load_explicit(&rwlock->cells[partition].rwlock,
    memory_order_relaxed);
  if (0 == val) {
    return partitioned_rwlock_wrlock(rwlock, partition);
  } else {
    return 1;
  }
#else
  return pthread_rwlock_trywrlock(&rwlock->cells[partition].rwlock);
#endif /* USE_LIBUV_RWLOCK */
} /* partitioned_rwlock_trywrlock() */

/* ------------------------------------------------------------------------- */

int
partitioned_rwlock_wrlock (
  partitioned_rwlock_t         *rwlock,
  const size_t                  partition
) {
  assert(NULL != rwlock);
  assert(partition < rwlock->partition_count);

#if defined(USE_LIBUV_RWLOCK)
  uv_rwlock_wrlock(&(rwlock->cells[partition].rwlock));
  rwlock->cells[partition].lock_type_held = PRWLOCK_TYPE_WRITE;
  return 0;
#elif defined(USE_ATOMICS) 
  while (1) {
    int32_t val = atomic_load_explicit(&rwlock->cells[partition].rwlock,
      memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(
      &rwlock->cells[partition].rwlock, &val, (val | (1<<31)),
      memory_order_acq_rel, memory_order_relaxed));
    val = INT32_MIN;
    while (atomic_compare_exchange_strong_explicit(
      &rwlock->cells[partition].rwlock, &val, -1, memory_order_acquire,
      memory_order_relaxed)) {
      val = INT32_MIN;
      asm("nop");
    }
    break;
  }
  rwlock->cells[partition].lock_type_held = PRWLOCK_TYPE_WRITE;
  return 0;
#else
  return pthread_rwlock_wrlock(&rwlock->cells[partition].rwlock);
#endif /* USE_LIBUV_RWLOCK */
} /* partitioned_rwlock_wrlock() */

/* ------------------------------------------------------------------------- */

int
partitioned_rwlock_unlock (
  partitioned_rwlock_t         *rwlock,
  const size_t                  partition
) {
  assert(NULL != rwlock);
  assert(partition < rwlock->partition_count);

#if defined(USE_LIBUV_RWLOCK)
  if (PRWLOCK_TYPE_READ == rwlock->cells[partition].lock_type_held) {
    uv_rwlock_rdunlock(&rwlock->cells[partition].rwlock);
  } else if (PRWLOCK_TYPE_WRITE == rwlock->cells[partition].lock_type_held) {
    uv_rwlock_wrunlock(&rwlock->cells[partition].rwlock);
  }
  return 0;
#elif defined(USE_ATOMICS) 
  if (PRWLOCK_TYPE_READ == rwlock->cells[partition].lock_type_held) {
    atomic_fetch_sub_explicit(&rwlock->cells[partition].rwlock, 1,
      memory_order_release);
  } else if (PRWLOCK_TYPE_WRITE == rwlock->cells[partition].lock_type_held) {
    atomic_store_explicit(&rwlock->cells[partition].rwlock, 0,
      memory_order_release);
  }
  return 0;
#else
  return pthread_rwlock_unlock(&rwlock->cells[partition].rwlock);
#endif /* USE_LIBUV_RWLOCK */
} /* partitioned_rwlock_unlock() */

/* :vi set ts=2 et sw=2: */


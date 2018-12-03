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

/* ========================================================================= */
/* -- MACROS --------------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE TYPES -------------------------------------------------------- */
/* ========================================================================= */

#ifdef USE_LIBUV_RWLOCK
typedef enum {
  PRWLOCK_TYPE_READ,
  PRWLOCK_TYPE_WRITE
} prwlock_type_t;
#endif /* USE_LIBUV_RWLOCK */

struct partitioned_rwlock_t {
  size_t                        partition_count;
#ifdef USE_LIBUV_RWLOCK
  uv_rwlock_t                  *partitions;
  prwlock_type_t               *lock_type_held;
#else
  pthread_rwlock_t             *partitions;
#endif /* USE_LIBUV_RWLOCK */
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
  partitioned_rwlock_t *newlock = calloc(1, sizeof(*newlock));
  newlock->partition_count = partition_count;
  newlock->partitions = calloc(partition_count, sizeof(*newlock->partitions));
#ifdef USE_LIBUV_RWLOCK
  newlock->lock_type_held = calloc(partition_count,
    sizeof(*newlock->lock_type_held));
#endif /* USE_LIBUV_RWLOCK */
  for (size_t ii = 0; ii < partition_count; ++ii) {
#ifdef USE_LIBUV_RWLOCK
    int rc = uv_rwlock_init(&(newlock->partitions[ii]));
#else
    int rc = pthread_rwlock_init(&(newlock->partitions[ii]), NULL);
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
#ifdef USE_LIBUV_RWLOCK
    uv_rwlock_destroy(&(rwlock->partitions[ii]));
#else
    int rc = pthread_rwlock_destroy(&(rwlock->partitions[ii]));
    if (0 != rc) {
      printf("init = %d\n", rc);
    }
#endif /* USE_LIBUV_RWLOCK */
  }
  free(rwlock->partitions);
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

#ifdef USE_LIBUV_RWLOCK
  uv_rwlock_rdlock(&(rwlock->partitions[partition]));
  rwlock->lock_type_held[partition] = PRWLOCK_TYPE_READ;
  return 0;
#else
  return pthread_rwlock_rdlock(&(rwlock->partitions[partition]));
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

#ifdef USE_LIBUV_RWLOCK
  int rc = uv_rwlock_tryrdlock(&(rwlock->partitions[partition]));
  if (0 == rc) {
    rwlock->lock_type_held[partition] = PRWLOCK_TYPE_READ;
  }
  return rc;
#else
  return pthread_rwlock_tryrdlock(&(rwlock->partitions[partition]));
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

#ifdef USE_LIBUV_RWLOCK
  int rc = uv_rwlock_trywrlock(&(rwlock->partitions[partition]));
  if (0 == rc) {
    rwlock->lock_type_held[partition] = PRWLOCK_TYPE_WRITE;
  }
  return rc;
#else
  return pthread_rwlock_trywrlock(&rwlock->partitions[partition]);
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

#ifdef USE_LIBUV_RWLOCK
  uv_rwlock_wrlock(&(rwlock->partitions[partition]));
  rwlock->lock_type_held[partition] = PRWLOCK_TYPE_WRITE;
  return 0;
#else
  return pthread_rwlock_wrlock(&rwlock->partitions[partition]);
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

#ifdef USE_LIBUV_RWLOCK
  if (PRWLOCK_TYPE_READ == rwlock->lock_type_held[partition]) {
    uv_rwlock_rdunlock(&rwlock->partitions[partition]);
  } else if (PRWLOCK_TYPE_WRITE == rwlock->lock_type_held[partition]) {
    uv_rwlock_wrunlock(&rwlock->partitions[partition]);
  }
  return 0;
#else
  return pthread_rwlock_unlock(&rwlock->partitions[partition]);
#endif /* USE_LIBUV_RWLOCK */
} /* partitioned_rwlock_unlock() */

/* :vi set ts=2 et sw=2: */


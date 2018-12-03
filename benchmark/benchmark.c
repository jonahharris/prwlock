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
#include <inttypes.h>
#include <assert.h>
#include <unistd.h>

#ifdef USE_LIBUV_RWLOCK
# include <uv.h>
#else
# include <pthread.h>
#endif /* USE_LIBUV_RWLOCK */

#include "prwlock.h"

/* ========================================================================= */
/* -- DEFINITIONS ---------------------------------------------------------- */
/* ========================================================================= */

#ifndef NUM_THREADS
# define NUM_THREADS            6
#endif /* NUM_THREADS */

#ifndef NUM_PARTITIONS
# define NUM_PARTITIONS         512
#endif /* NUM_PARTITIONS */

#ifndef NUM_ITERATIONS
# define NUM_ITERATIONS         1E6
#endif /* NUM_ITERATIONS */

/* ========================================================================= */
/* -- MACROS --------------------------------------------------------------- */
/* ========================================================================= */

#define HASH_JEN_MIX(a,b,c)                                                   \
do {                                                                          \
  a -= b; a -= c; a ^= ( c >> 13 );                                           \
  b -= c; b -= a; b ^= ( a << 8 );                                            \
  c -= a; c -= b; c ^= ( b >> 13 );                                           \
  a -= b; a -= c; a ^= ( c >> 12 );                                           \
  b -= c; b -= a; b ^= ( a << 16 );                                           \
  c -= a; c -= b; c ^= ( b >> 5 );                                            \
  a -= b; a -= c; a ^= ( c >> 3 );                                            \
  b -= c; b -= a; b ^= ( a << 10 );                                           \
  c -= a; c -= b; c ^= ( b >> 15 );                                           \
} while (0)

#define HASH_JEN(key,keylen,hashv)                                            \
do {                                                                          \
  unsigned _hj_i,_hj_j,_hj_k;                                                 \
  unsigned const char *_hj_key=(unsigned const char*)(key);                   \
  hashv = 0xfeedbeefu;                                                        \
  _hj_i = _hj_j = 0x9e3779b9u;                                                \
  _hj_k = (unsigned)(keylen);                                                 \
  while (_hj_k >= 12U) {                                                      \
    _hj_i +=    (_hj_key[0] + ( (unsigned)_hj_key[1] << 8 )                   \
        + ( (unsigned)_hj_key[2] << 16 )                                      \
        + ( (unsigned)_hj_key[3] << 24 ) );                                   \
    _hj_j +=    (_hj_key[4] + ( (unsigned)_hj_key[5] << 8 )                   \
        + ( (unsigned)_hj_key[6] << 16 )                                      \
        + ( (unsigned)_hj_key[7] << 24 ) );                                   \
    hashv += (_hj_key[8] + ( (unsigned)_hj_key[9] << 8 )                      \
        + ( (unsigned)_hj_key[10] << 16 )                                     \
        + ( (unsigned)_hj_key[11] << 24 ) );                                  \
                                                                              \
     HASH_JEN_MIX(_hj_i, _hj_j, hashv);                                       \
                                                                              \
     _hj_key += 12;                                                           \
     _hj_k -= 12U;                                                            \
  }                                                                           \
  hashv += (unsigned)(keylen);                                                \
  switch ( _hj_k ) {                                                          \
    case 11: hashv += ( (unsigned)_hj_key[10] << 24 ); /* FALLTHROUGH */      \
    case 10: hashv += ( (unsigned)_hj_key[9] << 16 );  /* FALLTHROUGH */      \
    case 9:  hashv += ( (unsigned)_hj_key[8] << 8 );   /* FALLTHROUGH */      \
    case 8:  _hj_j += ( (unsigned)_hj_key[7] << 24 );  /* FALLTHROUGH */      \
    case 7:  _hj_j += ( (unsigned)_hj_key[6] << 16 );  /* FALLTHROUGH */      \
    case 6:  _hj_j += ( (unsigned)_hj_key[5] << 8 );   /* FALLTHROUGH */      \
    case 5:  _hj_j += _hj_key[4];                      /* FALLTHROUGH */      \
    case 4:  _hj_i += ( (unsigned)_hj_key[3] << 24 );  /* FALLTHROUGH */      \
    case 3:  _hj_i += ( (unsigned)_hj_key[2] << 16 );  /* FALLTHROUGH */      \
    case 2:  _hj_i += ( (unsigned)_hj_key[1] << 8 );   /* FALLTHROUGH */      \
    case 1:  _hj_i += _hj_key[0];                                             \
  }                                                                           \
  HASH_JEN_MIX(_hj_i, _hj_j, hashv);                                          \
} while (0)

/* ========================================================================= */
/* -- PRIVATE TYPES -------------------------------------------------------- */
/* ========================================================================= */

typedef struct {
  partitioned_rwlock_t           *rwlock;
  size_t                          iteration_count;
  uint64_t                        sleep_in_microseconds;
  uint64_t                        mcg64_seed;
} prwlock_sample_thread_input_t;

typedef struct {
  uint64_t                        wait_count;
} prwlock_sample_thread_output_t;

typedef struct {
  prwlock_sample_thread_input_t   input;
  prwlock_sample_thread_output_t  output;
} prwlock_thread_context_t;

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

#ifdef USE_LIBUV_RWLOCK
void
#else
void *
#endif /* USE_LIBUV_RWLOCK */
random_reader_thread (
  void                         *arg
) {
  prwlock_thread_context_t *context = (prwlock_thread_context_t *) arg;
  partitioned_rwlock_t *rwlock = context->input.rwlock;
  size_t bucket_count = partitioned_rwlock_get_partition_count(rwlock);
  uint64_t random_id = context->input.mcg64_seed;
  uintptr_t wait_count = 0;
  unsigned hash_value = 0;
  unsigned hash_bucket = 0;

  for (int ii = 0; ii < NUM_ITERATIONS; ++ii) {
    random_id =
      ((164603309694725029ull * random_id) % 14738995463583502973ull);
    HASH_JEN(&random_id, sizeof(random_id), hash_value);
    hash_bucket = ((hash_value) & ((bucket_count) - 1U));
    if (0 != partitioned_rwlock_tryrdlock(rwlock, hash_bucket)) {
      ++wait_count;
      if (0 != partitioned_rwlock_rdlock(rwlock, hash_bucket)) {
        fprintf(stderr, "can't acquire read lock\n");
        exit(-1);
      }
    }

    if (0 < context->input.sleep_in_microseconds) {
      usleep(context->input.sleep_in_microseconds);
    }

    partitioned_rwlock_unlock(rwlock, hash_bucket);
  }

  context->output.wait_count = wait_count;

#ifndef USE_LIBUV_RWLOCK
  return NULL;
#endif /* !USE_LIBUV_RWLOCK */
} /* random_reader_thread() */

/* ------------------------------------------------------------------------- */

#ifdef USE_LIBUV_RWLOCK
void
#else
void *
#endif /* USE_LIBUV_RWLOCK */
random_writer_thread (
  void                         *arg
) {
  prwlock_thread_context_t *context = (prwlock_thread_context_t *) arg;
  partitioned_rwlock_t *rwlock = context->input.rwlock;
  size_t bucket_count = partitioned_rwlock_get_partition_count(rwlock);
  uint64_t random_id = context->input.mcg64_seed;
  uintptr_t wait_count = 0;
  unsigned hash_value = 0;
  unsigned hash_bucket = 0;

  for (int ii = 0; ii < NUM_ITERATIONS; ++ii) {
    random_id =
      ((164603309694725029ull * random_id) % 14738995463583502973ull);
    HASH_JEN(&random_id, sizeof(random_id), hash_value);
    hash_bucket = ((hash_value) & ((bucket_count) - 1U));
    if (0 != partitioned_rwlock_trywrlock(rwlock, hash_bucket)) {
      ++wait_count;
      if (0 != partitioned_rwlock_wrlock(rwlock, hash_bucket)) {
        fprintf(stderr, "can't acquire write lock\n");
        exit(-1);
      }
    }

    if (0 < context->input.sleep_in_microseconds) {
      usleep(context->input.sleep_in_microseconds);
    }

    partitioned_rwlock_unlock(rwlock, hash_bucket);
  }

  context->output.wait_count = wait_count;

#ifndef USE_LIBUV_RWLOCK
  return NULL;
#endif /* !USE_LIBUV_RWLOCK */
} /* random_writer_thread() */

/* ========================================================================= */
/* -- PUBLIC METHODS ------------------------------------------------------- */
/* ========================================================================= */

int
main (
  int                           argc,
  char                        **argv
) {
  partitioned_rwlock_t *rwlock;
  partitioned_rwlock_init(&rwlock, NUM_PARTITIONS);

  prwlock_thread_context_t thread_context[NUM_THREADS];
#ifdef USE_LIBUV_RWLOCK
  uv_thread_t threads[NUM_THREADS];
#else
  pthread_t threads[NUM_THREADS];
#endif /* USE_LIBUV_RWLOCK */

  for (int ii = 0; ii < NUM_THREADS; ++ii) {
    memset(&thread_context[ii], 0, sizeof(prwlock_thread_context_t));
    thread_context[ii].input.rwlock = rwlock;
    thread_context[ii].input.iteration_count = NUM_ITERATIONS;
    thread_context[ii].input.sleep_in_microseconds = 1;
    thread_context[ii].input.mcg64_seed = (ii + 1);

    if (0 == (ii % 2)) {
#ifdef USE_LIBUV_RWLOCK
      uv_thread_create(&threads[ii], random_reader_thread,
        &thread_context[ii]);
#else
      pthread_create(&threads[ii], NULL, random_reader_thread,
        &thread_context[ii]);
#endif /* USE_LIBUV_RWLOCK */
    } else {
      thread_context[ii].input.sleep_in_microseconds *= 2;
#ifdef USE_LIBUV_RWLOCK
      uv_thread_create(&threads[ii], random_writer_thread,
        &thread_context[ii]);
#else
      pthread_create(&threads[ii], NULL, random_writer_thread,
        &thread_context[ii]);
#endif /* USE_LIBUV_RWLOCK */
    }
  }

  for (int ii = 0; ii < NUM_THREADS; ++ii) {
#ifdef USE_LIBUV_RWLOCK
    int rc = uv_thread_join(&threads[ii]);
#else
    int rc = pthread_join(threads[ii], NULL);
#endif /* USE_LIBUV_RWLOCK */
    if (0 == (ii % 2)) {
      printf("reader thread encountered %"PRIu64" waits\n",
        thread_context[ii].output.wait_count);
    } else {
      printf("writer thread encountered %"PRIu64" waits\n",
        thread_context[ii].output.wait_count);
    }
  }

  partitioned_rwlock_destroy(rwlock);

  return 0;

} /* main() */

/* :vi set ts=2 et sw=2: */


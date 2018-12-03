#ifndef PRWLOCK_H
#define PRWLOCK_H
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

#include <stdlib.h>
#ifdef USE_LIBUV_RWLOCK
# include <uv.h>
#else
# include <pthread.h>
#endif /* USE_LIBUV_RWLOCK */

/* ========================================================================= */
/* -- DEFINITIONS ---------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- MACROS --------------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE TYPES -------------------------------------------------------- */
/* ========================================================================= */

typedef struct partitioned_rwlock_t partitioned_rwlock_t;

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

int partitioned_rwlock_init (partitioned_rwlock_t **rwlock,
  size_t partition_count);
int partitioned_rwlock_destroy (partitioned_rwlock_t *rwlock);
size_t partitioned_rwlock_get_partition_count (partitioned_rwlock_t *rwlock);
int partitioned_rwlock_rdlock (partitioned_rwlock_t *rwlock,
  const size_t partition);
int partitioned_rwlock_tryrdlock (partitioned_rwlock_t *rwlock,
  const size_t partition);
int partitioned_rwlock_wrlock (partitioned_rwlock_t *rwlock,
  const size_t partition);
int partitioned_rwlock_trywrlock (partitioned_rwlock_t *rwlock,
  const size_t partition);
int partitioned_rwlock_unlock (partitioned_rwlock_t *rwlock,
  const size_t partition);

#endif /* PRWLOCK_H */

/* :vi set ts=2 et sw=2: */


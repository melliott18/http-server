/**
 * @file rwlock.h
 *
 * Reader/writer lock with selectable contention policy.
 *
 * @author Andrew Quinn, Mitchell Elliott, and Gurpreet Dhillon.
 */

#pragma once

#include <stdint.h>

/** Opaque, non-recursive lock. Lock upgrades and downgrades are unsupported. */
typedef struct rwlock rwlock_t;

/** READERS/WRITERS favor the corresponding waiting operation. N_WAY allows a
 * batch of up to n reader admissions before a waiting writer is admitted.
 */
typedef enum { READERS, WRITERS, N_WAY } PRIORITY;

/** Allocate and initialize a lock with policy p.
 * p must be a PRIORITY value. For N_WAY, n must be in 1..INT_MAX; otherwise n is
 * ignored. Returns NULL on allocation or pthread initialization failure.
 */
rwlock_t *rwlock_new(PRIORITY p, uint32_t n);

/** Release lock storage and set *rw to NULL. NULL arguments are accepted.
 * All holders and waiting threads must have finished using the lock first.
 */
void rwlock_delete(rwlock_t **rw);

/** Acquire shared access, blocking without a timeout until the policy permits
 * entry. rw must be initialized and the caller must not already hold it.
 */
void reader_lock(rwlock_t *rw);

/** Release shared access previously acquired by this thread. */
void reader_unlock(rwlock_t *rw);

/** Acquire exclusive access, blocking without a timeout until the policy permits
 * entry. rw must be initialized and the caller must not already hold it.
 */
void writer_lock(rwlock_t *rw);

/** Release exclusive access previously acquired by this thread. */
void writer_unlock(rwlock_t *rw);

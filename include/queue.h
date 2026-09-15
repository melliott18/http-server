/**
 * @file queue.h
 *
 * Bounded, blocking FIFO queue for concurrent producers and consumers.
 *
 * @author Andrew Quinn
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/** Opaque queue. Stored pointers remain owned by the caller. */
typedef struct queue queue_t;

/** Allocate a queue holding at most size pointers.
 * size must be positive. Returns NULL for invalid capacity, allocation failure,
 * or failure to initialize the POSIX semaphores. Requires Linux semaphore
 * support; there is no shutdown or cancellation operation.
 */
queue_t *queue_new(int size);

/** Release queue storage and set *q to NULL. NULL arguments are accepted.
 * No other thread may use or wait on the queue during deletion. Stored elements
 * are not freed; drain the queue first if they require cleanup.
 */
void queue_delete(queue_t **q);

/** Append elem, blocking until capacity is available. elem may be NULL.
 * Returns false for a NULL queue or semaphore wait failure, otherwise true.
 * Interrupted waits are retried; no timeout is imposed.
 */
bool queue_push(queue_t *q, void *elem);

/** Remove the oldest element into *elem, blocking until an element is available.
 * Returns false for a NULL queue/output pointer or semaphore wait failure,
 * otherwise true. Interrupted waits are retried; no timeout is imposed.
 */
bool queue_pop(queue_t *q, void **elem);

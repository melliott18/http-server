#include "queue.h"

#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>

// queue has FIFO properties and should support multi producer and multi consumer
struct queue {
    int size;
    int start;
    int end;
    sem_t push;
    sem_t pop;
    sem_t free;
    sem_t cont;
    void **data;
};

int succ(int n, int s) {
    return (n + 1) % s;
}

// Allocates a queue with size `size`
queue_t *queue_new(int size) {
    if (size <= 0) {
        return NULL;
    }
    queue_t *q = malloc(sizeof(*q));
    if (!q) {
        return NULL;
    }
    q->size = size;
    q->start = 0;
    q->end = 0;
    q->data = malloc(sizeof(void *) * q->size);
    if (!q->data) {
        free(q);
        return NULL;
    }
    if (sem_init(&q->push, 0, 1) < 0) {
        goto failed;
    }
    if (sem_init(&q->pop, 0, 1) < 0) {
        goto failed_pop;
    }
    if (sem_init(&q->free, 0, q->size) < 0) {
        goto failed_free;
    }
    if (sem_init(&q->cont, 0, 0) < 0) {
        goto failed_cont;
    }
    return q;

failed_cont:
    sem_destroy(&q->free);
failed_free:
    sem_destroy(&q->pop);
failed_pop:
    sem_destroy(&q->push);
failed:
    free(q->data);
    free(q);
    return NULL;
}

// frees a queue (should assume the queue is empty)
void queue_delete(queue_t **q) {
    if (!q || !*q) {
        return;
    }

    sem_destroy(&q[0]->push);
    sem_destroy(&q[0]->pop);
    sem_destroy(&q[0]->free);
    sem_destroy(&q[0]->cont);
    free(q[0]->data);
    free(q[0]);

    *q = NULL;
}

// add an element to the queue
// blocks if queue is full
static int wait_sem(sem_t *sem) {
    int result;
    do {
        result = sem_wait(sem);
    } while (result < 0 && errno == EINTR);
    return result;
}

bool queue_push(queue_t *q, void *elem) {
    if (!q) {
        return false;
    }
    if (wait_sem(&q->free) < 0) {
        return false;
    }
    if (wait_sem(&q->push) < 0) {
        sem_post(&q->free);
        return false;
    }

    q->data[q->end] = elem;
    q->end = succ(q->end, q->size);

    sem_post(&q->cont);
    sem_post(&q->push);

    return true;
}

// remove an element from the queue
// blocks if the queue is empty
bool queue_pop(queue_t *q, void **elem) {
    if (!q || !elem) {
        return false;
    }

    if (wait_sem(&q->cont) < 0) {
        return false;
    }
    if (wait_sem(&q->pop) < 0) {
        sem_post(&q->cont);
        return false;
    }

    *elem = q->data[q->start];
    q->start = succ(q->start, q->size);

    sem_post(&q->free);
    sem_post(&q->pop);

    return true;
}

/**
 * @File rwlock.c
 *
 * Implementation of the reader/writer lock.
 *
 * @author Mitchell Elliott
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "rwlock.h"

#ifdef DEBUG
#define debug(...)                                                                                 \
    do {                                                                                           \
        fprintf(stderr, "[%s:%s():%d]\t", __FILE__, __func__, __LINE__);                           \
        fprintf(stderr, __VA_ARGS__);                                                              \
        fprintf(stderr, "\n");                                                                     \
    } while (0);
#else
#define debug(...) ((void) 0)
#endif

struct rwlock {
    PRIORITY p; // Priority of the rwlock
    int n; // Number of readers in nway mode
    pthread_mutex_t mu; // rwlock mutex
    pthread_cond_t rcv; // Reader condition variable
    pthread_cond_t wcv; // Writer condition variable
    int areaders; // Number of active readers
    int awriters; // Number of active writers
    int wreaders; // Number of waiting readers
    int wwriters; // Number of waiting writers
    int nreaders; // Number of readers in nway mode
};

int reader_wait(rwlock_t *rwlock) {
    switch (rwlock->p) {
    case READERS: return rwlock->awriters;
    case WRITERS: return (rwlock->awriters || rwlock->wwriters);
    case N_WAY: return (rwlock->awriters || (rwlock->wwriters && (rwlock->nreaders >= rwlock->n)));
    default: return (rwlock->areaders || rwlock->awriters);
    }
}

int writer_wait(rwlock_t *rwlock) {
    switch (rwlock->p) {
    case READERS: return (rwlock->areaders || rwlock->awriters || rwlock->wreaders);
    case WRITERS: return (rwlock->areaders || rwlock->awriters);
    case N_WAY:
        return (rwlock->areaders || rwlock->awriters
                || (rwlock->wreaders && (rwlock->nreaders < rwlock->n)));
    default: return (rwlock->areaders || rwlock->awriters);
    }
}

void rwlock_wakeup(rwlock_t *rwlock) {
    if (rwlock->wreaders) {
        pthread_cond_broadcast(&rwlock->rcv);
    }

    if (rwlock->wwriters) {
        pthread_cond_broadcast(&rwlock->wcv);
    }

    return;
}

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *rwlock = (rwlock_t *) malloc(sizeof(rwlock_t));
    if (rwlock == NULL) {
        debug("malloc failed");
        return NULL;
    }

    rwlock->p = p;
    rwlock->n = n;

    int rc = pthread_mutex_init(&rwlock->mu, NULL);
    if (rc) {
        debug("pthread_mutex_init failed: %d\n", rc);
        goto cleanup;
    }

    rc = pthread_cond_init(&rwlock->rcv, NULL);
    if (rc) {
        debug("pthread_cond_init failed: %d\n", rc);
        pthread_mutex_destroy(&rwlock->mu);
        goto cleanup;
    }

    rc = pthread_cond_init(&rwlock->wcv, NULL);
    if (rc) {
        debug("pthread_cond_init failed: %d\n", rc);
        pthread_cond_destroy(&rwlock->rcv);
        pthread_mutex_destroy(&rwlock->mu);
        goto cleanup;
    }

    rwlock->areaders = rwlock->awriters = rwlock->wreaders = rwlock->wwriters = rwlock->nreaders
        = 0;
    return rwlock;

cleanup:
    free(rwlock);
    rwlock = NULL;
    return NULL;
}

void rwlock_delete(rwlock_t **rwlock) {
    if (!rwlock || !*rwlock) {
        return;
    }
    int rc = pthread_mutex_destroy(&(*rwlock)->mu);
    if (rc) {
        debug("pthread_mutex_destroy failed: %d\n", rc);
    }

    rc = pthread_cond_destroy(&(*rwlock)->rcv);
    if (rc) {
        debug("pthread_cond_destroy failed: %d\n", rc);
    }

    rc = pthread_cond_destroy(&(*rwlock)->wcv);
    if (rc) {
        debug("pthread_cond_destroy failed: %d\n", rc);
    }

    free(*rwlock);
    *rwlock = NULL;
    return;
}

void reader_lock(rwlock_t *rwlock) {
    pthread_mutex_lock(&rwlock->mu);

    rwlock->wreaders++;

    while (reader_wait(rwlock)) {
        pthread_cond_wait(&rwlock->rcv, &rwlock->mu);
    }

    rwlock->wreaders--;
    rwlock->areaders++;
    rwlock->nreaders++;

    pthread_mutex_unlock(&rwlock->mu);
    return;
}

void reader_unlock(rwlock_t *rwlock) {
    pthread_mutex_lock(&rwlock->mu);

    rwlock->areaders--;

    rwlock_wakeup(rwlock);

    pthread_mutex_unlock(&rwlock->mu);
    return;
}

void writer_lock(rwlock_t *rwlock) {
    pthread_mutex_lock(&rwlock->mu);

    rwlock->wwriters++;

    while (writer_wait(rwlock)) {
        pthread_cond_wait(&rwlock->wcv, &rwlock->mu);
    }

    rwlock->wwriters--;
    rwlock->awriters++;

    pthread_mutex_unlock(&rwlock->mu);
    return;
}

void writer_unlock(rwlock_t *rwlock) {
    pthread_mutex_lock(&rwlock->mu);

    rwlock->awriters--;
    rwlock->nreaders = 0;

    rwlock_wakeup(rwlock);

    pthread_mutex_unlock(&rwlock->mu);
    return;
}

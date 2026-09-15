#include "hashtable.h"
#include "list.h"
#include "rwlock.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HashTable {
    uint32_t length;
    ListNode_t **heads;
};

uint64_t hash(char *str) {
    uint64_t hash = 5381;
    int32_t c = (*str++);

    while (c) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        c = (*str++);
    }

    return hash;
}

HashTable_t *ht_create(uint32_t length) {
    if (length == 0) {
        return NULL;
    }
    HashTable_t *ht = (HashTable_t *) malloc(sizeof(HashTable_t));

    if (ht) {
        ht->length = length;
        ht->heads = (ListNode_t **) calloc(length, sizeof(ListNode_t *));
        if (!ht->heads) {
            free(ht);
            return NULL;
        }
    }

    return ht;
}

void ht_delete(HashTable_t *ht) {
    if (ht && ht->heads) {
        for (uint32_t i = 0; i < ht->length; i++) {
            list_delete(ht->heads[i]);
        }

        free(ht->heads);
        free(ht);
        ht = NULL;
    }

    return;
}

rwlock_t *ht_lookup(HashTable_t *ht, char *uri) {
    ListNode_t *ln;
    rwlock_t *rwlock = NULL;

    if (ht && ht->heads) {
        uint32_t index = (uint32_t) hash(uri);
        index = index % ht->length;
        ln = list_lookup(&(ht->heads[index]), uri);
        rwlock = list_node_get_rwlock(ln);
    }

    return rwlock;
}

rwlock_t *ht_insert(HashTable_t *ht, char *uri) {
    ListNode_t *ln;
    rwlock_t *rwlock = NULL;

    if (uri) {
        if (ht && ht->heads) {
            uint32_t index = (uint32_t) hash(uri);
            index = index % ht->length;
            ln = list_insert(&(ht->heads[index]), uri);
            rwlock = list_node_get_rwlock(ln);
        }
    }

    return rwlock;
}

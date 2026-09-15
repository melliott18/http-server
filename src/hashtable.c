#include "hashtable.h"
#include "list.h"

#include <stdlib.h>

struct HashTable {
    uint32_t length;
    ListNode_t **heads;
};

static uint64_t hash(const char *str) {
    uint64_t value = 5381;
    while (*str) {
        value = value * 33 + (unsigned char) *str++;
    }
    return value;
}

HashTable_t *ht_create(uint32_t length) {
    if (length == 0) {
        return NULL;
    }
    HashTable_t *ht = malloc(sizeof(*ht));
    if (!ht) {
        return NULL;
    }
    ht->length = length;
    ht->heads = calloc(length, sizeof(*ht->heads));
    if (!ht->heads) {
        free(ht);
        return NULL;
    }
    return ht;
}

void ht_delete(HashTable_t *ht) {
    if (!ht) {
        return;
    }
    for (uint32_t i = 0; i < ht->length; i++) {
        list_delete(ht->heads[i]);
    }
    free(ht->heads);
    free(ht);
}

rwlock_t *ht_acquire(HashTable_t *ht, const char *uri) {
    if (!ht || !uri) {
        return NULL;
    }
    return list_acquire(&ht->heads[hash(uri) % ht->length], uri);
}

bool ht_release(HashTable_t *ht, const char *uri) {
    if (!ht || !uri) {
        return false;
    }
    return list_release(&ht->heads[hash(uri) % ht->length], uri);
}

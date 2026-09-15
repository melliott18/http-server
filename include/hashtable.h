#ifndef NIL
#define NIL (void *) 0
#endif

#ifndef __HASH_H__
#define __HASH_H__

#include <inttypes.h>
#include "list.h"

typedef struct HashTable HashTable_t;

HashTable_t *ht_create(uint32_t length);

void ht_delete(HashTable_t *ht);

rwlock_t *ht_lookup(HashTable_t *ht, char *uri);

rwlock_t *ht_insert(HashTable_t *ht, char *uri);

#endif

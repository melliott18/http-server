#pragma once

#include "rwlock.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct HashTable HashTable_t;

// Create a filename-to-lock registry; return NULL for zero buckets or allocation failure.
HashTable_t *ht_create(uint32_t length);

// Destroy an idle registry. All acquired references must have been released.
void ht_delete(HashTable_t *ht);

// Registry calls require external serialization. Acquire a reference before using
// or waiting on the returned lock; NULL means invalid input or allocation failure.
// Entries exist only while at least one caller holds a reference.
rwlock_t *ht_acquire(HashTable_t *ht, const char *uri);

// Release a reference after unlocking. The last release destroys the entry.
// Return false if the registry or filename has no corresponding reference.
bool ht_release(HashTable_t *ht, const char *uri);

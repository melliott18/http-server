#pragma once

#include "rwlock.h"

#include <stdbool.h>

typedef struct ListNode ListNode_t;

// Bucket operations require external serialization and share the registry's
// acquire-before-use, release-after-unlock contract.
rwlock_t *list_acquire(ListNode_t **head, const char *uri);
bool list_release(ListNode_t **head, const char *uri);

// Destroy an idle bucket; all acquired references must already be released.
void list_delete(ListNode_t *head);

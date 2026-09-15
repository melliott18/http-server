#ifndef NIL
#define NIL (void *) 0
#endif

#ifndef __LIST_H__
#define __LIST_H__

#include "rwlock.h"
#include <inttypes.h>
#include <stdbool.h>

typedef struct ListNode ListNode_t;

ListNode_t *list_node_create(char *uri);

void list_node_delete(ListNode_t *ln);

rwlock_t *list_node_get_rwlock(ListNode_t *ln);

ListNode_t *list_create(void);

void list_delete(ListNode_t *head);

ListNode_t *list_lookup(ListNode_t **head, char *uri);

ListNode_t *list_insert(ListNode_t **head, char *uri);

#endif

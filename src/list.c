#include "list.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct ListNode {
    rwlock_t *rwlock;
    char *uri;
    size_t references;
    ListNode_t *next;
};

static void list_node_delete(ListNode_t *node) {
    rwlock_delete(&node->rwlock);
    free(node->uri);
    free(node);
}

rwlock_t *list_acquire(ListNode_t **head, const char *uri) {
    if (!head || !uri) {
        return NULL;
    }
    for (ListNode_t *node = *head; node; node = node->next) {
        if (strcmp(node->uri, uri) == 0) {
            if (node->references == SIZE_MAX) {
                return NULL;
            }
            node->references++;
            return node->rwlock;
        }
    }

    ListNode_t *node = malloc(sizeof(*node));
    if (!node) {
        return NULL;
    }
    node->rwlock = rwlock_new(READERS, 0);
    node->uri = strdup(uri);
    if (!node->rwlock || !node->uri) {
        list_node_delete(node);
        return NULL;
    }
    node->references = 1;
    node->next = *head;
    *head = node;
    return node->rwlock;
}

bool list_release(ListNode_t **head, const char *uri) {
    if (!head || !uri) {
        return false;
    }
    for (ListNode_t **link = head; *link; link = &(*link)->next) {
        ListNode_t *node = *link;
        if (strcmp(node->uri, uri) == 0) {
            node->references--;
            if (node->references == 0) {
                *link = node->next;
                list_node_delete(node);
            }
            return true;
        }
    }
    return false;
}

void list_delete(ListNode_t *head) {
    while (head) {
        ListNode_t *node = head;
        head = head->next;
        list_node_delete(node);
    }
}

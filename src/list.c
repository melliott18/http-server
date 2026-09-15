#include "list.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct ListNode {
    rwlock_t *rwlock;
    char *uri;
    ListNode_t *next;
};

ListNode_t *list_node_create(char *uri) {
    ListNode_t *ln = (ListNode_t *) malloc(sizeof(ListNode_t));

    if (ln) {
        ln->rwlock = rwlock_new(READERS, 0);
        ln->uri = strdup(uri);
        ln->next = NULL;
        if (!ln->rwlock || !ln->uri) {
            if (ln->rwlock) {
                rwlock_delete(&ln->rwlock);
            }
            free(ln->uri);
            free(ln);
            return NULL;
        }
    }

    return ln;
}

void list_node_delete(ListNode_t *ln) {
    if (ln) {
        if (ln->rwlock) {
            rwlock_delete(&ln->rwlock);
        }
        free(ln->uri);
        free(ln);
        ln = NULL;
    }

    return;
}

rwlock_t *list_node_get_rwlock(ListNode_t *ln) {
    rwlock_t *rwlock = NULL;

    if (ln) {
        rwlock = ln->rwlock;
    }

    return rwlock;
}

ListNode_t *list_create(void) {
    ListNode_t *ln = (ListNode_t *) malloc(sizeof(ListNode_t));

    if (ln) {
        ln->rwlock = NULL;
        ln->uri = NULL;
        ln->next = NULL;
    }

    return ln;
}

void list_delete(ListNode_t *head) {
    while (head) {
        ListNode_t *ln = head;
        head = head->next;
        list_node_delete(ln);
    }

    return;
}

ListNode_t *list_lookup(ListNode_t **head, char *uri) {
    ListNode_t *ln = *head;

    while (ln) {
        if (strcmp(ln->uri, uri) == 0) {
            return ln;
        }

        ln = ln->next;
    }

    return NULL;
}

ListNode_t *list_insert(ListNode_t **head, char *uri) {
    ListNode_t *ln = NULL;

    if (uri) {
        ln = list_lookup(head, uri);
        if (!ln) {
            ln = list_node_create(uri);

            if (ln) {
                ln->next = *head;
                *head = ln;
            }
        }
    }

    return ln;
}

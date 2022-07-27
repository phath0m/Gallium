/*
 * list.c - Linked list implementation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/list.h>

bool
GaIter_Next(list_iter_t *iterp, void **val)
{
    struct list_elem *elem = (struct list_elem*)*iterp;

    if (elem) {
        *val = elem->val;
        *iterp = (list_iter_t)elem->next;
    }

    return (elem != NULL);
}

bool
GaIter_Peek(list_iter_t *iterp, void **val)
{
    struct list_elem *elem = (struct list_elem*)*iterp;

    if (elem) {
        *val = elem->val;
    }

    return (elem != NULL);
}

bool
GaIter_PeekEx(list_iter_t *iterp, int n, void **val)
{
    struct list_elem *elem = (struct list_elem*)*iterp;

    for (int i = 0; i < n && elem; i++) {
        elem = elem->next;
    }

    if (elem) {
        *val = elem->val;
    }

    return (elem != NULL);
}

void
GaList_Destroy(struct list *listp, list_free_t free_func, void *state)
{
    GaList_Fini(listp, free_func, state);
    free(listp);
}

void
GaList_Fini(struct list *listp, list_free_t free_func, void *state)
{
    struct list_elem *cur = listp->head;

    while (cur) {
        struct list_elem *prev = cur;

        if (free_func) {
            free_func(prev->val, state);
        }

        cur = cur->next;

        free(prev);
    }

    memset(listp, 0, sizeof(*listp));
}

struct list *
GaList_New()
{
    struct list *listp = calloc(sizeof(struct list), 1);

    return listp;
}

void
GaList_Push(struct list *listp, void *val)
{
    struct list_elem *elem = calloc(sizeof(struct list_elem), 1);
    elem->val = val;
    
    if (!listp->head) {
        listp->head = elem;
        listp->tail = elem;
    } else {
        elem->prev = listp->tail;
        elem->prev->next = elem;
        listp->tail = elem;
    }

    listp->count++;
}

void
GaList_Unshift(struct list *listp, void *val)
{
    struct list_elem *elem = calloc(sizeof(struct list_elem), 1);
    elem->val = val;
    elem->next = listp->head;

    if (!listp->head) {
        listp->head = elem;
        listp->tail = elem;
    } else {
        listp->head = elem;
    }
    
    listp->count++;
}

void *
GaList_Head(struct list *listp)
{
    struct list_elem *cur = listp->head;
    return cur->val;
}

void
GaList_GetIter(struct list *listp, list_iter_t *iterp)
{
    *iterp = (list_iter_t)listp->head;
}

bool
GaList_Remove(struct list *listp, void *val, list_free_t free_func, void *state)
{
    struct list_elem *cur = listp->head;

    while (cur) {
        if (cur->val == val) {
            if (cur->prev) {
                cur->prev->next = cur->next;
            }

            if (cur->next) {
                cur->next->prev = cur->prev;
            }

            if (cur == listp->head) {
                listp->head = cur->next;
            }

            if (cur == listp->tail) {
                listp->tail = cur->prev;
            }

            if (free_func) {
                free_func(cur->val, state);
            }

            free(cur);

            listp->count--;
            return true;
        }

        cur = cur->next;
    }
    return false;
}

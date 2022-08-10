/*
 * linkedlist_impl.c - Internal Linked list implementation
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
_Ga_iter_next(_Ga_iter_t *iterp, void **val)
{
    _Ga_list_elem_t *elem = (_Ga_list_elem_t*)*iterp;

    if (elem) {
        *val = elem->val;
        *iterp = (_Ga_iter_t)elem->next;
    }

    return (elem != NULL);
}

bool
_Ga_iter_peek(_Ga_iter_t *iterp, void **val)
{
    _Ga_list_elem_t *elem = (_Ga_list_elem_t*)*iterp;

    if (elem) {
        *val = elem->val;
    }

    return (elem != NULL);
}

bool
_Ga_iter_peek_ex(_Ga_iter_t *iterp, int n, void **val)
{
    _Ga_list_elem_t *elem = (_Ga_list_elem_t*)*iterp;

    for (int i = 0; i < n && elem; i++) {
        elem = elem->next;
    }

    if (elem) {
        *val = elem->val;
    }

    return (elem != NULL);
}

void
_Ga_list_destroy(_Ga_list_t *listp, list_free_t free_func, void *state)
{
    _Ga_list_fini(listp, free_func, state);
    free(listp);
}

void
_Ga_list_fini(_Ga_list_t *listp, list_free_t free_func, void *state)
{
    _Ga_list_elem_t *cur = listp->head;

    while (cur) {
        _Ga_list_elem_t *prev = cur;

        if (free_func) {
            free_func(prev->val, state);
        }

        cur = cur->next;
        free(prev);
    }

    memset(listp, 0, sizeof(*listp));
}

_Ga_list_t *
_Ga_list_new()
{
    _Ga_list_t *listp = calloc(sizeof(_Ga_list_t), 1);

    return listp;
}

void
_Ga_list_push(_Ga_list_t *listp, void *val)
{
    _Ga_list_elem_t *elem = calloc(sizeof(_Ga_list_elem_t), 1);
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
_Ga_list_unshift(_Ga_list_t *listp, void *val)
{
    _Ga_list_elem_t *elem = calloc(sizeof(_Ga_list_elem_t), 1);
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
_Ga_list_head(_Ga_list_t *listp)
{
    _Ga_list_elem_t *cur = listp->head;
    return cur->val;
}

void
_Ga_list_get_iter(_Ga_list_t *listp, _Ga_iter_t *iterp)
{
    *iterp = (_Ga_iter_t)listp->head;
}

bool
_Ga_list_remove(_Ga_list_t *listp, void *val, list_free_t free_func, void *state)
{
    _Ga_list_elem_t *cur = listp->head;

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

bool
_Ga_list_contains(_Ga_list_t *listp, void *val)
{
    _Ga_list_elem_t *cur = listp->head;

    while (cur) {
        if (cur->val == val) 
            return true;
        cur = cur->next;
    }
    return false;
}
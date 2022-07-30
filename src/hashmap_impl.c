/*
 * hashmap_impl.c - Internal Hashmap implementation
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gallium/dict.h>
#include <gallium/list.h>

/* arguments to pass to function responsible for freeing each element inside the dictionary */
struct dict_destroy_state {
    dict_free_t func;
    void    *   state;
};

static void
dict_destroy_func(void *p, void *s)
{
    struct dict_kvp *kvp = p;
    struct dict_destroy_state *state = s;

    if (state->func) {
        state->func(kvp->val, state->state);
    }

    free(kvp);
}

void
GaHashMap_Destroy(struct dict *dictp, dict_free_t free_func, void *statep)
{
    GaHashMap_Fini(dictp, free_func, statep);
    free(dictp);
}

void
GaHashMap_Fini(struct dict *dictp, dict_free_t free_func, void *statep)
{
    struct dict_destroy_state state = {
        .func   =   free_func,
        .state  =   statep
    };

    for (int i = 0; i < dictp->hash_size; i++) {
        struct list *listp = &dictp->buckets[i];

        if (LIST_COUNT(listp) > 0) {
            GaLinkedList_Fini(listp, NULL, NULL);
        }
    }

    GaLinkedList_Fini(&dictp->values, dict_destroy_func, &state);

    if (dictp->buckets) free(dictp->buckets);
}

struct dict *
GaHashMap_New()
{
    struct dict *dictp = calloc(sizeof(struct dict), 1);
    return dictp;
}

bool
GaHashMap_HasKey(struct dict *dictp, const char *key)
{
    if (!dictp->count) return false;

    uint32_t hash = DICT_BUCKET_INDEX(dictp, DICT_HASH(key));

    if (LIST_COUNT(&dictp->buckets[hash]) == 0) {
        return false;
    }

    struct list *listp = &dictp->buckets[hash];
    struct dict_kvp *kvp;
    list_iter_t iter;
    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&kvp)) {
        if (strncmp(kvp->key, key, sizeof(kvp->key)) == 0) {
            return true;
        }
    }

    return false;
}

bool
GaHashMap_Get(struct dict *dictp, const char *key, void **res)
{
    return dict_get_prehashed(dictp, DICT_HASH(key), key, res);
}

void
GaHashMap_GetIter(struct dict *dictp, list_iter_t *iter)
{
    GaLinkedList_GetIter(&dictp->values, iter);
}

bool
GaHashMap_Remove(struct dict *dictp, const char *key, dict_free_t free_func, void *state)
{
    uint32_t hash = DICT_BUCKET_INDEX(dictp, DICT_HASH(key));

    if (LIST_COUNT(&dictp->buckets[hash]) == 0) {
        return false;
    }

    struct list *listp = &dictp->buckets[hash];
    struct dict_kvp *match = NULL;
    struct dict_kvp *kvp;
    list_iter_t iter;

    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&kvp)) {
        if (strncmp(kvp->key, key, sizeof(kvp->key)) == 0) {
            match = kvp;
            break;
        }
    }

    if (match) {
        GaLinkedList_Remove(listp, match, NULL, NULL);
        GaLinkedList_Remove(&dictp->values, match, NULL, NULL);

        if (free_func) {
            free_func(match->val, state);
        }

        dictp->count--;
        free(match);
        return true;
    }

    return false;
}


static void
dict_grow(struct dict *dictp, int new_hash_size)
{
    if (!dictp->buckets) {
        dictp->hash_size = new_hash_size;
        dictp->buckets = calloc(1, sizeof(struct list)*new_hash_size);
        return;
    }

    /* First, clear out any existing buckets */
    for (int i = 0; i < dictp->hash_size; i++) {
        struct list *listp = &dictp->buckets[i];

        if (LIST_COUNT(listp) > 0) GaLinkedList_Fini(listp, NULL, NULL);
    }
    /*
     * Resize buffer
     */
    dictp->buckets = realloc(dictp->buckets, new_hash_size*sizeof(struct list));
    memset(&dictp->buckets[dictp->hash_size], 0, (new_hash_size-dictp->hash_size) * sizeof(struct list));
    dictp->hash_size = new_hash_size;

    /*
     * Now re-add each entry
     */
    struct dict_kvp *cur_kvp;
    list_iter_t iter;
    GaLinkedList_GetIter(&dictp->values, &iter);

    while (GaIter_Next(&iter, (void**)&cur_kvp)) {
        uint32_t hash = DICT_BUCKET_INDEX(dictp, DICT_HASH(cur_kvp->key));
        GaLinkedList_Push(&dictp->buckets[hash], cur_kvp);
    }
}

void
GaHashMap_Set(struct dict *dictp, const char *key, void *val)
{
    if (!dictp->buckets || dictp->count > (dictp->hash_size >> 2) * 3) {
        if (dictp->hash_size == 0) dictp->hash_size = 32;
        dict_grow(dictp, dictp->hash_size*2);
    }

    uint32_t hash = DICT_BUCKET_INDEX(dictp, DICT_HASH(key));
    struct list *listp = &dictp->buckets[hash];
    struct dict_kvp *cur_kvp;

    list_iter_t iter;
    GaLinkedList_GetIter(listp, &iter);

    while (GaIter_Next(&iter, (void**)&cur_kvp)) {
        if (strncmp(cur_kvp->key, key, sizeof(cur_kvp->key)) == 0) {
            cur_kvp->val = val;
            return;
        }
    }

    struct dict_kvp *kvp = malloc(sizeof(struct dict_kvp));
    kvp->val = val;
    strncpy(kvp->key, key, sizeof(kvp->key)-1);

    GaLinkedList_Push(listp, kvp);
    GaLinkedList_Push(&dictp->values, kvp);

    dictp->count++;
}
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
    _Ga_dict_kvp_t *kvp = p;
    struct dict_destroy_state *state = s;

    if (state->func) {
        state->func(kvp->val, state->state);
    }

    free(kvp);
}

void
_Ga_hashmap_destroy(_Ga_dict_t *dictp, dict_free_t free_func, void *statep)
{
    _Ga_hashmap_fini(dictp, free_func, statep);
    free(dictp);
}

void
_Ga_hashmap_fini(_Ga_dict_t *dictp, dict_free_t free_func, void *statep)
{
    struct dict_destroy_state state = {
        .func   =   free_func,
        .state  =   statep
    };

    for (int i = 0; i < dictp->hash_size; i++) {
        _Ga_list_t *listp = &dictp->buckets[i];

        if (_Ga_LIST_COUNT(listp) > 0) {
            _Ga_list_fini(listp, NULL, NULL);
        }
    }

    _Ga_list_fini(&dictp->values, dict_destroy_func, &state);

    if (dictp->buckets) free(dictp->buckets);
}

_Ga_dict_t *
_Ga_hashmap_new()
{
    _Ga_dict_t *dictp = calloc(sizeof(_Ga_dict_t), 1);
    return dictp;
}

bool
_Ga_hashmap_contains(_Ga_dict_t *dictp, const char *key)
{
    if (!dictp->count) return false;

    uint32_t hash = _Ga_DICT_BUCKET_INDEX(dictp, _Ga_DICT_HASH(key));

    if (_Ga_LIST_COUNT(&dictp->buckets[hash]) == 0) {
        return false;
    }

    _Ga_list_t *listp = &dictp->buckets[hash];
    _Ga_dict_kvp_t *kvp;
    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&kvp)) {
        if (strncmp(kvp->key, key, sizeof(kvp->key)) == 0) {
            return true;
        }
    }

    return false;
}

bool
_Ga_hashmap_get(_Ga_dict_t *dictp, const char *key, void **res)
{
    return _Ga_hashmap_get_prehashed(dictp, _Ga_DICT_HASH(key), key, res);
}

void
_Ga_hashmap_getiter(_Ga_dict_t *dictp, _Ga_iter_t *iter)
{
    _Ga_list_get_iter(&dictp->values, iter);
}

bool
_Ga_hashmap_remove(_Ga_dict_t *dictp, const char *key, dict_free_t free_func, void *state)
{
    uint32_t hash = _Ga_DICT_BUCKET_INDEX(dictp, _Ga_DICT_HASH(key));

    if (_Ga_LIST_COUNT(&dictp->buckets[hash]) == 0) {
        return false;
    }

    _Ga_list_t *listp = &dictp->buckets[hash];
    _Ga_dict_kvp_t *match = NULL;
    _Ga_dict_kvp_t *kvp;
    _Ga_iter_t iter;

    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&kvp)) {
        if (strncmp(kvp->key, key, sizeof(kvp->key)) == 0) {
            match = kvp;
            break;
        }
    }

    if (match) {
        _Ga_list_remove(listp, match, NULL, NULL);
        _Ga_list_remove(&dictp->values, match, NULL, NULL);

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
dict_grow(_Ga_dict_t *dictp, int new_hash_size)
{
    if (!dictp->buckets) {
        dictp->hash_size = new_hash_size;
        dictp->buckets = calloc(1, sizeof(_Ga_list_t)*new_hash_size);
        return;
    }

    /* First, clear out any existing buckets */
    for (int i = 0; i < dictp->hash_size; i++) {
        _Ga_list_t *listp = &dictp->buckets[i];

        if (_Ga_LIST_COUNT(listp) > 0) _Ga_list_fini(listp, NULL, NULL);
    }
    /*
     * Resize buffer
     */
    dictp->buckets = realloc(dictp->buckets, new_hash_size*sizeof(_Ga_list_t));
    memset(&dictp->buckets[dictp->hash_size], 0, (new_hash_size-dictp->hash_size) * sizeof(_Ga_list_t));
    dictp->hash_size = new_hash_size;

    /*
     * Now re-add each entry
     */
    _Ga_dict_kvp_t *cur_kvp;
    _Ga_iter_t iter;
    _Ga_list_get_iter(&dictp->values, &iter);

    while (_Ga_iter_next(&iter, (void**)&cur_kvp)) {
        uint32_t hash = _Ga_DICT_BUCKET_INDEX(dictp, _Ga_DICT_HASH(cur_kvp->key));
        _Ga_list_push(&dictp->buckets[hash], cur_kvp);
    }
}

void
_Ga_hashmap_set(_Ga_dict_t *dictp, const char *key, void *val)
{
    if (!dictp->buckets || dictp->count > (dictp->hash_size >> 2) * 3) {
        if (dictp->hash_size == 0) dictp->hash_size = 32;
        dict_grow(dictp, dictp->hash_size*2);
    }

    uint32_t hash = _Ga_DICT_BUCKET_INDEX(dictp, _Ga_DICT_HASH(key));
    _Ga_list_t *listp = &dictp->buckets[hash];
    _Ga_dict_kvp_t *cur_kvp;

    _Ga_iter_t iter;
    _Ga_list_get_iter(listp, &iter);

    while (_Ga_iter_next(&iter, (void**)&cur_kvp)) {
        if (strncmp(cur_kvp->key, key, sizeof(cur_kvp->key)) == 0) {
            cur_kvp->val = val;
            return;
        }
    }

    _Ga_dict_kvp_t *kvp = malloc(sizeof(_Ga_dict_kvp_t));
    kvp->val = val;
    strncpy(kvp->key, key, sizeof(kvp->key)-1);

    _Ga_list_push(listp, kvp);
    _Ga_list_push(&dictp->values, kvp);

    dictp->count++;
}
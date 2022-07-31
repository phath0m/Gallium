#ifndef _DS_DICT_H
#define _DS_DICT_H

#include <string.h>
#include <gallium/list.h>

typedef struct _Ga_dict {
    _Ga_list_t      values;
    _Ga_list_t  *   buckets;
    int             hash_size;
    int             count;
} _Ga_dict_t;

typedef struct _Ga_dict_kvp {
    char            key[128];
    void    *       val;
} _Ga_dict_kvp_t;

typedef void (*dict_free_t) (void *, void*);

void            _Ga_hashmap_destroy(_Ga_dict_t *, dict_free_t, void *);
void            _Ga_hashmap_fini(_Ga_dict_t *, dict_free_t, void *);
_Ga_dict_t  *   _Ga_hashmap_new();
bool            _Ga_hashmap_contains(_Ga_dict_t *, const char *);
bool            _Ga_hashmap_get(_Ga_dict_t *, const char *, void **);
void            _Ga_hashmap_getiter(_Ga_dict_t *, _Ga_iter_t *);
bool            _Ga_hashmap_remove(_Ga_dict_t *, const char *, dict_free_t,
                                   void *);
void            _Ga_hashmap_set(_Ga_dict_t *, const char *, void *);

#define _Ga_DICT_BUCKET_INDEX(dict, hash) ((hash) & ((dict)->hash_size-1))

static inline uint32_t
_Ga_DICT_HASH(const char *str)
{
    uint32_t res = 2166136261;
    while (*str) {
        res = (int)(*str++) + (res << 6) + (res << 16) - res;
    }
    return res;
}

static inline bool
_Ga_hashmap_get_prehashed(_Ga_dict_t *dictp, uint32_t hash, const char *key,
                          void **res)
{
    if (!dictp->count) return false;

    hash = _Ga_DICT_BUCKET_INDEX(dictp, hash);

    if (_Ga_LIST_COUNT(&dictp->buckets[hash]) == 0) {
        return false;
    }

    _Ga_list_t *listp = &dictp->buckets[hash];
    _Ga_dict_kvp_t *kvp;
    _Ga_iter_t iter;
    _Ga_LIST_GET_ITER(listp, &iter);

    while (_Ga_ITER_NEXT(&iter, (void**)&kvp)) {
        if (strncmp(kvp->key, key, sizeof(kvp->key)) == 0) {
            *res = kvp->val;
            return true;
        }
    }

    return false;
}

#endif

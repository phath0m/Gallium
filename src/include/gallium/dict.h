#ifndef _DS_DICT_H
#define _DS_DICT_H

#include <string.h>
#include <gallium/list.h>

struct dict {
    //_Ga_list_t     entries[DICT_HASH_SIZE];    /* the actual hashmap */
    _Ga_list_t      values;
    _Ga_list_t  *   buckets;
    int             hash_size;
    int             count;
};

struct dict_kvp {
    char            key[128];
    void    *       val;
};

typedef void (*dict_free_t) (void *, void*);

void            GaHashMap_Destroy(struct dict *, dict_free_t, void *);
void            GaHashMap_Fini(struct dict *, dict_free_t, void *);
struct dict *   GaHashMap_New();
bool            GaHashMap_HasKey(struct dict *, const char *);
bool            GaHashMap_Get(struct dict *, const char *, void **);
void            GaHashMap_GetIter(struct dict *, _Ga_iter_t *);
bool            GaHashMap_Remove(struct dict *, const char *, dict_free_t, void *);
void            GaHashMap_Set(struct dict *, const char *, void *);

#define DICT_BUCKET_INDEX(dict, hash) ((hash) & ((dict)->hash_size-1))

static inline uint32_t
DICT_HASH(const char *str)
{
    uint32_t res = 2166136261;
    while (*str) {
        res = (int)(*str++) + (res << 6) + (res << 16) - res;
    }
    return res;
}

static inline bool
dict_get_prehashed(struct dict *dictp, uint32_t hash, const char *key, void **res)
{
    if (!dictp->count) return false;

    hash = DICT_BUCKET_INDEX(dictp, hash);

    if (_Ga_LIST_COUNT(&dictp->buckets[hash]) == 0) {
        return false;
    }

    _Ga_list_t *listp = &dictp->buckets[hash];
    struct dict_kvp *kvp;
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

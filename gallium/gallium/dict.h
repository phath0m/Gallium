#ifndef _DS_DICT_H
#define _DS_DICT_H

#include <string.h>
#include <gallium/list.h>

#define DICT_HASH_SIZE  16

struct dict {
    struct list     entries[DICT_HASH_SIZE];    /* the actual hashmap */
    struct list     values;
    int             count;
};

struct dict_kvp {
    char            key[128];
    void    *       val;
};

typedef void (*dict_free_t) (void *, void*);

void            dict_destroy(struct dict *, dict_free_t, void *);
void            dict_fini(struct dict *, dict_free_t, void *);
struct dict *   dict_new();
bool            dict_has_key(struct dict *, const char *);
bool            dict_get(struct dict *, const char *, void **);
void            dict_get_iter(struct dict *, list_iter_t *);
bool            dict_remove(struct dict *, const char *, dict_free_t, void *);
void            dict_set(struct dict *, const char *, void *);

static inline uint32_t
DICT_HASH(const char *str)
{
    uint32_t res = 2166136261;

    while (*str) {
        res = (int)(*str++) + (res << 6) + (res << 16) - res;
    }

    return res & (DICT_HASH_SIZE-1);
}

static inline bool
dict_get_prehashed(struct dict *dictp, uint32_t hash, const char *key, void **res)
{
    if (LIST_COUNT(&dictp->entries[hash]) == 0) {
        return false;
    }

    struct list *listp = &dictp->entries[hash];
    struct dict_kvp *kvp;
    list_iter_t iter;

    LIST_GET_ITER(listp, &iter);

    while (ITER_NEXT_ELEM(&iter, (void**)&kvp)) {
        if (strncmp(kvp->key, key, sizeof(kvp->key)) == 0) {
            *res = kvp->val;
            return true;
        }
    }

    return false;
}


#endif

#ifndef _DS_POOL_H
#define _DS_POOL_H

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct pool_ent {
    struct pool_ent *   next;
    char                data[];

};

struct pool {
    struct pool_ent *   free_items;
    size_t              size;
    int                 max_items;
};

__attribute__((always_inline))
static inline void *
POOL_GET(struct pool *pp)
{
    struct pool_ent *ent = pp->free_items;

    if (ent) {
        pp->free_items = ent->next;
    } else {
        //ent = calloc(1, sizeof(struct pool_ent) + pp->size);
        ent = malloc(sizeof(struct pool_ent) + pp->size);
    }

    return ent->data;
}

__attribute__((always_inline))
static inline void
POOL_PUT(struct pool *pp, void *data)
{
    struct pool_ent *ent = &((struct pool_ent*)data)[-1];
    ent->next = pp->free_items;
    pp->free_items = ent;
    //memset(ent->data, 0, pp->size);
}


#endif

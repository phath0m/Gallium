#ifndef _DS_POOL_H
#define _DS_POOL_H

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct pool_ent {
    struct pool_ent *   next;
    struct pool_ent *   prev;
    char                data[];

};

struct pool {
    struct pool_ent *   free_items;
    struct pool_ent *   allocated_items;
    size_t              size;
    int                 num_items;
    int                 max_items;
};

__attribute__((always_inline))
static inline void *
GaPool_GET(struct pool *pp)
{
    struct pool_ent *ent = pp->free_items;
    if (ent) {
        pp->free_items = ent->next;
    } else {
        ent = malloc(sizeof(struct pool_ent) + pp->size);
    }

    ent->next = pp->allocated_items;
    ent->prev = NULL;

    if (pp->allocated_items) {
        pp->allocated_items->prev = ent;
    }

    pp->allocated_items = ent;
    pp->num_items++;
    return ent->data;
}

__attribute__((always_inline))
static inline void
GaPool_PUT(struct pool *pp, void *data)
{
    struct pool_ent *ent = &((struct pool_ent*)data)[-1];

    if (ent->prev) {
        ent->prev->next = ent->next;
    }

    if (ent->next) {
        ent->next->prev = ent->prev;
    }

    if (ent == pp->allocated_items) {
        pp->allocated_items = ent->next;
    }

    ent->next = pp->free_items;
    pp->free_items = ent;
    pp->num_items--;
}
#endif

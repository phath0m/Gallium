#ifndef _DS_LIST_H
#define _DS_LIST_H

#include <stdbool.h>
#include <stddef.h>

#define LIST_COUNT(l)   ((l)->count)

struct list_elem {
    void                *   val;
    struct list_elem    *   next;
    struct list_elem    *   prev;
};

struct list {
    struct list_elem    *   head;
    struct list_elem    *   tail;
    int                     count;
};

typedef void (*list_free_t) (void *, void *);
typedef struct list_elem *  list_iter_t;

bool            iter_next_elem(list_iter_t *, void **);
bool            iter_peek_elem(list_iter_t *, void **);
bool            iter_peek_n_elem(list_iter_t *, int, void **);
void            list_destroy(struct list *, list_free_t, void *);
void            list_fini(struct list *, list_free_t, void *);
struct list *   list_new();
void            list_append(struct list *, void *);
void            list_append_front(struct list *, void *);
void    *       list_first(struct list *);
void            list_get_iter(struct list *, list_iter_t *);
bool            list_remove(struct list *, void *, list_free_t, void *);

#define LIST_INIT(p) \
    (p).head = NULL; \
    (p).tail = NULL; \
    (p).count = 0;



static inline bool
ITER_NEXT_ELEM(list_iter_t *iterp, void **val)
{
    struct list_elem *elem = (struct list_elem*)*iterp;

    if (elem) {
        *val = elem->val;
        *iterp = (list_iter_t)elem->next;
    }

    return (elem != NULL);
}

static inline void
LIST_GET_ITER(struct list *listp, list_iter_t *iterp)
{
    *iterp = (list_iter_t)listp->head;
}

#endif

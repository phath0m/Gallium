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

bool            GaIter_Next(list_iter_t *, void **);
bool            GaIter_Peek(list_iter_t *, void **);
bool            GaIter_PeekEx(list_iter_t *, int, void **);
void            GaList_Destroy(struct list *, list_free_t, void *);
void            GaList_Fini(struct list *, list_free_t, void *);
struct list *   GaList_New();
void            GaList_Push(struct list *, void *);
void            GaList_Unshift(struct list *, void *);
void    *       GaList_Head(struct list *);
void            GaList_GetIter(struct list *, list_iter_t *);
bool            GaList_Remove(struct list *, void *, list_free_t, void *);

#define LIST_INIT(p) \
    (p).head = NULL; \
    (p).tail = NULL; \
    (p).count = 0;

static inline bool
GaIter_NEXT(list_iter_t *iterp, void **val)
{
    struct list_elem *elem = (struct list_elem*)*iterp;

    if (elem) {
        *val = elem->val;
        *iterp = (list_iter_t)elem->next;
    }

    return (elem != NULL);
}

static inline void
GaList_GET_ITER(struct list *listp, list_iter_t *iterp)
{
    *iterp = (list_iter_t)listp->head;
}

#endif

#ifndef _GALLIUM_LIST_H
#define _GALLIUM_LIST_H

#include <stdbool.h>
#include <stddef.h>

#define _Ga_LIST_COUNT(l)   ((l)->count)

typedef struct _Ga_list_elem _Ga_list_elem_t;
typedef struct _Ga_list_elem {
    void                *   val;
    _Ga_list_elem_t     *   next;
    _Ga_list_elem_t     *   prev;
} _Ga_list_elem_t;

typedef struct _ga_list {
    _Ga_list_elem_t     *   head;
    _Ga_list_elem_t     *   tail;
    int                     count;
} _Ga_list_t;

typedef void (*list_free_t) (void *, void *);
typedef _Ga_list_elem_t *  _Ga_iter_t;

bool            _Ga_iter_next(_Ga_iter_t *, void **);
bool            _Ga_iter_peek(_Ga_iter_t *, void **);
bool            _Ga_iter_peek_ex(_Ga_iter_t *, int, void **);
void            _Ga_list_destroy(_Ga_list_t *, list_free_t, void *);
void            _Ga_list_fini(_Ga_list_t *, list_free_t, void *);
_Ga_list_t  *   _Ga_list_new();
void            _Ga_list_push(_Ga_list_t *, void *);
void            _Ga_list_unshift(_Ga_list_t *, void *);
void        *   _Ga_list_head(_Ga_list_t *);
void            _Ga_list_get_iter(_Ga_list_t *, _Ga_iter_t *);
bool            _Ga_list_remove(_Ga_list_t *, void *, list_free_t, void *);
bool            _Ga_list_contains(_Ga_list_t *, void *);

#define _Ga_LIST_INIT(p) \
    (p).head = NULL; \
    (p).tail = NULL; \
    (p).count = 0;

static inline bool
_Ga_ITER_NEXT(_Ga_iter_t *iterp, void **val)
{
    _Ga_list_elem_t *elem = (_Ga_list_elem_t*)*iterp;

    if (elem) {
        *val = elem->val;
        *iterp = (_Ga_iter_t)elem->next;
    }

    return (elem != NULL);
}

static inline void
_Ga_LIST_GET_ITER(_Ga_list_t *listp, _Ga_iter_t *iterp)
{
    *iterp = (_Ga_iter_t)listp->head;
}

#endif

#ifndef _GALLIUM_VEC_H
#define _GALLIUM_VEC_H

#define VEC_INITIAL_SIZE    64

typedef void (*vec_free_t) (void *, void *);

struct vec {
    void **     cells;
    size_t      avail_cells;
    size_t      used_cells;
};

void        GaVec_Init(struct vec *);
int         GaVec_Append(struct vec *, void *);
void        GaVec_Fini(struct vec *, vec_free_t, void *);

/* these are unsafe... */
#define VEC_FAST_GET(p, i)      ((p)->cells[(i)])
#define VEC_FAST_SET(p, i, v)   ((p)->cells[(i)]) = (v)

#endif

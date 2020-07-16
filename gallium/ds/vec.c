#include <stdlib.h>
#include <gallium/vec.h>
// delete
#include <stdio.h>

void
vec_init(struct vec *vecp)
{
    vecp->cells = calloc(sizeof(void*)*VEC_INITIAL_SIZE, 1);
    vecp->avail_cells = VEC_INITIAL_SIZE;
    vecp->used_cells = 0;
}

int
vec_add(struct vec *vecp, void *val)
{
    if (vecp->used_cells >= vecp->avail_cells) {
        vecp->used_cells *= 2;
        vecp->cells = realloc(vecp->cells, sizeof(void*)*vecp->used_cells);
    }

    vecp->cells[vecp->used_cells++] = val;

    return vecp->used_cells - 1;
}


void
vec_fini(struct vec *vecp, vec_free_t free_func, void *statep)
{
    for (int i = 0; i < vecp->used_cells; i++) {
        free_func(vecp->cells[i], statep);
    }
    
    free(vecp->cells);
}

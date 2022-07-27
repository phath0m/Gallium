/*
 * vec.c - Vector implementation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <gallium/vec.h>

void
GaVec_Init(struct vec *vecp)
{
    vecp->cells = calloc(sizeof(void*)*VEC_INITIAL_SIZE, 1);
    vecp->avail_cells = VEC_INITIAL_SIZE;
    vecp->used_cells = 0;
}

int
GaVec_Append(struct vec *vecp, void *val)
{
    if (vecp->used_cells >= vecp->avail_cells) {
        vecp->avail_cells *= 2;
        vecp->cells = realloc(vecp->cells, sizeof(void*)*vecp->avail_cells);
    }

    vecp->cells[vecp->used_cells++] = val;

    return vecp->used_cells - 1;
}

void
GaVec_Fini(struct vec *vecp, vec_free_t free_func, void *statep)
{
    for (int i = 0; i < vecp->used_cells; i++) {
        free_func(vecp->cells[i], statep);
    }
    
    free(vecp->cells);
}

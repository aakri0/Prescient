/* t10_pathological.c
 *
 * Complexity pattern: the deliberate worst case — a 4-level loop nest
 * combined with heavy pointer aliasing and a nested-struct type. The
 * innermost loop is dense with loads, stores and pointer arithmetic.
 *
 * Why it is interesting to the model: this stacks every cost driver at
 * once (loop depth, alias-query density, type complexity) to anchor the
 * VERY HIGH end of the training distribution.
 */

#include <stdlib.h>

struct Grid {
    int *cells;
    int *scratch;
    int width;
    int height;
};

struct Field {
    struct Grid grids[2];
    double *weights;
    int generations;
};

void evolve(struct Grid *g, int steps) {
    for (int s = 0; s < steps; s++) {
        for (int y = 0; y < g->height; y++) {
            for (int x = 0; x < g->width; x++) {
                int acc = 0;
                for (int k = 0; k < 4; k++) {
                    int ny = (y + k) % g->height;
                    int nx = (x + k) % g->width;
                    acc += g->cells[ny * g->width + nx];
                    g->scratch[y * g->width + x] = acc;
                }
                g->cells[y * g->width + x] = g->scratch[y * g->width + x];
            }
        }
    }
}

double simulate_field(struct Field *f) {
    double energy = 0.0;
    for (int gen = 0; gen < f->generations; gen++) {
        for (int gi = 0; gi < 2; gi++) {
            struct Grid *g = &f->grids[gi];
            for (int y = 0; y < g->height; y++) {
                for (int x = 0; x < g->width; x++) {
                    int idx = y * g->width + x;
                    g->cells[idx] += g->scratch[idx];
                    energy += f->weights[gi] * (double)g->cells[idx];
                }
            }
        }
    }
    return energy;
}

int cross_blur(int *dst, const int *src, int w, int h) {
    int touched = 0;
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int si = (y + dy) * w + (x + dx);
                    dst[y * w + x] += src[si];
                    touched++;
                }
            }
        }
    }
    return touched;
}

/* test02_loops_eval.c
 *
 * Held-out evaluation case (NOT used in training): nested-loop numeric
 * kernels, including a 4-level loop nest in convolve(). Exercises the
 * high end of the loop-depth feature.
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

#define DIM 24

static void fill(int *m, int dim, int seed) {
    for (int i = 0; i < dim; i++)
        for (int j = 0; j < dim; j++)
            m[i * dim + j] = (i * 7 + j * 13 + seed) & 0xFF;
}

static long convolve(const int *m, int dim) {
    long sum = 0;
    for (int i = 1; i < dim - 1; i++)
        for (int j = 1; j < dim - 1; j++)
            for (int di = -1; di <= 1; di++)
                for (int dj = -1; dj <= 1; dj++)
                    sum += m[(i + di) * dim + (j + dj)];
    return sum;
}

int main(void) {
    int grid[DIM * DIM];
    long total = 0;
    for (int run = 0; run < 3000; run++) {
        fill(grid, DIM, run);
        total += convolve(grid, DIM);
    }
    printf("test02 total=%ld\n", total);
    return 0;
}

/* t40_heavy_compute.c — Deliberately heavy computation functions.
 * Pattern: HIGH-tier anchoring — very high instruction counts, deep nesting.
 */

#include <stdlib.h>

void matrix_chain_order(const int *dims, int n, int *cost, int *split) {
    /* O(n^3) DP for optimal matrix chain multiplication order */
    for (int i = 0; i < n; i++) cost[i * n + i] = 0;
    for (int len = 2; len <= n; len++) {
        for (int i = 0; i <= n - len; i++) {
            int j = i + len - 1;
            cost[i * n + j] = 0x7FFFFFFF;
            for (int k = i; k < j; k++) {
                int q = cost[i * n + k] + cost[(k+1) * n + j]
                        + dims[i] * dims[k+1] * dims[j+1];
                if (q < cost[i * n + j]) {
                    cost[i * n + j] = q;
                    split[i * n + j] = k;
                }
            }
        }
    }
}

void dense_sgemm(float *C, const float *A, const float *B, int M, int N, int K) {
    /* Simple triple-loop SGEMM: C[M][N] = A[M][K] * B[K][N] */
    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0;
            for (int k = 0; k < K; k++)
                sum += A[i * K + k] * B[k * N + j];
            C[i * N + j] = sum;
        }
}

void jacobi_iteration(double *next, const double *curr, int n) {
    /* 2D Jacobi relaxation step on n x n grid */
    for (int i = 1; i < n - 1; i++)
        for (int j = 1; j < n - 1; j++)
            next[i * n + j] = 0.25 * (
                curr[(i-1)*n+j] + curr[(i+1)*n+j] +
                curr[i*n+(j-1)] + curr[i*n+(j+1)]
            );
}

int life_step(const int *grid, int *next, int w, int h) {
    /* Conway's Game of Life — one generation */
    int changes = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int neighbors = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dy == 0 && dx == 0) continue;
                    int ny = y + dy, nx = x + dx;
                    if (ny >= 0 && ny < h && nx >= 0 && nx < w)
                        neighbors += grid[ny * w + nx];
                }
            int alive = grid[y * w + x];
            int next_state = (alive && (neighbors == 2 || neighbors == 3))
                          || (!alive && neighbors == 3);
            next[y * w + x] = next_state;
            if (next_state != alive) changes++;
        }
    return changes;
}

void histogram_equalize(unsigned char *img, int n, int max_val) {
    int hist[256], cdf[256];
    for (int i = 0; i < 256; i++) hist[i] = 0;
    for (int i = 0; i < n; i++) hist[img[i]]++;
    cdf[0] = hist[0];
    for (int i = 1; i < 256; i++) cdf[i] = cdf[i-1] + hist[i];
    int cdf_min = 0;
    for (int i = 0; i < 256; i++) if (cdf[i] > 0) { cdf_min = cdf[i]; break; }
    for (int i = 0; i < n; i++) {
        int v = (cdf[img[i]] - cdf_min) * (max_val - 1) / (n - cdf_min);
        img[i] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
}

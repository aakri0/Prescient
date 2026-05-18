/* t02_nested_loops.c
 *
 * Complexity pattern: a classic matrix multiply with a 3-level loop nest
 * operating over pointer arguments, plus two shallower loop functions.
 *
 * Why it is interesting to the model: loop nesting depth is the single
 * strongest predictor of optimisation cost — LICM, loop unrolling and
 * vectorisation all scale super-linearly with depth. This is a HIGH-tier
 * training sample.
 */

void matrix_multiply(const int *a, const int *b, int *c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int sum = 0;
            for (int k = 0; k < n; k++)
                sum += a[i * n + k] * b[k * n + j];
            c[i * n + j] = sum;
        }
    }
}

void matrix_scale(int *m, int n, int factor) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            m[i * n + j] *= factor;
    }
}

int matrix_trace(const int *m, int n) {
    int t = 0;
    for (int i = 0; i < n; i++)
        t += m[i * n + i];
    return t;
}

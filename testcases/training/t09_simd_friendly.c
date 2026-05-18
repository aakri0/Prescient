/* t09_simd_friendly.c
 *
 * Complexity pattern: simple, regular array loops with no loop-carried
 * dependencies and no aliasing hazards between distinct buffers — the
 * kind of code the loop and SLP vectorisers handle well.
 *
 * Why it is interesting to the model: these loops are cheap to analyse
 * but trigger the vectoriser, so they exercise the vectorisation passes
 * without heavy alias or control-flow cost. This is a LOW-MEDIUM tier
 * sample.
 */

int array_sum(const int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        s += a[i];
    return s;
}

void array_scale(int *a, int n, int factor) {
    for (int i = 0; i < n; i++)
        a[i] *= factor;
}

void array_add(int *out, const int *x, const int *y, int n) {
    for (int i = 0; i < n; i++)
        out[i] = x[i] + y[i];
}

int array_dot(const int *x, const int *y, int n) {
    int acc = 0;
    for (int i = 0; i < n; i++)
        acc += x[i] * y[i];
    return acc;
}

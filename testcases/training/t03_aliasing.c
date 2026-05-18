/* t03_aliasing.c
 *
 * Complexity pattern: heavy pointer aliasing. The same buffers are read
 * and written through multiple pointer arguments that may overlap, so
 * the compiler cannot prove non-aliasing without analysis.
 *
 * Why it is interesting to the model: every load/store pair forces the
 * alias-analysis framework to issue queries during LICM, GVN and
 * vectorisation. The structural alias proxy (memory-op density) is high,
 * making this a HIGH-tier sample.
 */

void copy_overlap(int *dst, const int *src, int n) {
    for (int i = 0; i < n; i++)
        dst[i] = src[i] + src[(i + 1) % n];
}

int sum_through_two(const int *p, const int *q, int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        s += p[i] + q[i];
    return s;
}

void accumulate(int *acc, const int *a, const int *b, int n) {
    for (int i = 0; i < n; i++)
        acc[i] += a[i] * b[i];
}

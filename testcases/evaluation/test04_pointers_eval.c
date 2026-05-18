/* test04_pointers_eval.c
 *
 * Held-out evaluation case (NOT used in training): pointer-heavy buffer
 * processing with loads, stores and pointer arithmetic. Exercises the
 * alias-proxy-density feature.
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

#define LEN 256

static void scale_shift(int *buf, int len, int factor, int shift) {
    for (int i = 0; i < len; i++)
        buf[i] = buf[i] * factor + shift;
}

static long reduce_pairs(const int *a, const int *b, int len) {
    long sum = 0;
    for (int i = 0; i < len; i++)
        sum += (long)a[i] * b[len - 1 - i];
    return sum;
}

int main(void) {
    int x[LEN];
    int y[LEN];
    long total = 0;
    for (int run = 0; run < 20000; run++) {
        for (int i = 0; i < LEN; i++) {
            x[i] = (i + run) & 0xFF;
            y[i] = (i ^ run) & 0xFF;
        }
        scale_shift(x, LEN, 3, run & 7);
        total += reduce_pairs(x, y, LEN);
    }
    printf("test04 total=%ld\n", total);
    return 0;
}

/* test01_arith_eval.c
 *
 * Held-out evaluation case (NOT used in training): scalar arithmetic leaf
 * functions plus a small driver loop. Low static complexity — exercises
 * the low end of the model's prediction range.
 *
 * Self-contained runnable program: main() exercises every function and
 * prints a deterministic checksum so baseline and adaptive builds can be
 * compared for correctness.
 */
#include <stdio.h>

static int poly(int x) {
    return 3 * x * x + 2 * x + 7;
}

static int blend(int a, int b) {
    return ((a * 5) - (b * 3)) ^ (a + b);
}

static int fold(int seed, int steps) {
    int acc = seed;
    for (int i = 0; i < steps; i++)
        acc = blend(acc, poly(i & 0x3F));
    return acc;
}

int main(void) {
    long checksum = 0;
    for (int run = 0; run < 4000; run++)
        checksum += fold(run, 1024);
    printf("test01 checksum=%ld\n", checksum);
    return 0;
}

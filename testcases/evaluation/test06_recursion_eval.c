/* test06_recursion_eval.c
 *
 * Held-out evaluation case (NOT used in training): recursive functions
 * that the inliner cannot fully expand. Exercises the call-site feature
 * and recursion handling.
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

static long collatz_len(long n) {
    if (n <= 1)
        return 0;
    if (n & 1)
        return 1 + collatz_len(3 * n + 1);
    return 1 + collatz_len(n / 2);
}

static int gcd(int a, int b) {
    if (b == 0)
        return a;
    return gcd(b, a % b);
}

int main(void) {
    long acc = 0;
    for (int i = 1; i < 60000; i++) {
        acc += collatz_len(i);
        acc += gcd(i, i / 2 + 1);
    }
    printf("test06 acc=%ld\n", acc);
    return 0;
}

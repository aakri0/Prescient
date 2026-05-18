/* test03_branches_eval.c
 *
 * Held-out evaluation case (NOT used in training): branch-heavy control
 * flow with no loops in the leaf functions. Exercises the cyclomatic
 * complexity feature.
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

static int classify(int v) {
    if (v < 0)        return 0;
    else if (v < 16)  return 1;
    else if (v < 64)  return 2;
    else if (v < 128) return 3;
    else if (v < 192) return 4;
    else if (v < 240) return 5;
    else              return 6;
}

static int score(int a, int b, int c) {
    int s = 0;
    if (a > b)  s += 3; else s -= 1;
    if (b > c)  s += 5; else s -= 2;
    if (a > c)  s += 7; else s -= 3;
    if ((a ^ b) & 1) s *= 2;
    if ((b ^ c) & 2) s += classify(a);
    return s;
}

int main(void) {
    long acc = 0;
    for (int i = 0; i < 3000000; i++)
        acc += score(i & 0xFF, (i >> 3) & 0xFF, (i >> 6) & 0xFF);
    printf("test03 acc=%ld\n", acc);
    return 0;
}

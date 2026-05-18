/* test08_mixed_eval.c
 *
 * Held-out evaluation case (NOT used in training): a mixed workload
 * combining a switch, loops, branches, calls and memory traffic in one
 * larger program. Exercises several features at once.
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

#define N 128

static int transform(int v, int mode) {
    switch (mode & 3) {
        case 0:  return v + 1;
        case 1:  return v * 2;
        case 2:  return v ^ 0x5A;
        default: return (v >> 1) + 3;
    }
}

static long process(int *buf, int len) {
    long sum = 0;
    for (int i = 0; i < len; i++) {
        buf[i] = transform(buf[i], i);
        if (buf[i] & 1)
            sum += buf[i];
        else
            sum -= buf[i] / 2;
    }
    for (int i = 0; i < len; i++)
        for (int j = i + 1; j < len; j++)
            if (buf[i] > buf[j])
                sum += 1;
    return sum;
}

int main(void) {
    int buf[N];
    long total = 0;
    for (int run = 0; run < 8000; run++) {
        for (int i = 0; i < N; i++)
            buf[i] = (i * 31 + run) & 0xFF;
        total += process(buf, N);
    }
    printf("test08 total=%ld\n", total);
    return 0;
}

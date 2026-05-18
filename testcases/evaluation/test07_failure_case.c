/* test07_failure_case.c
 *
 * FAILURE CASE — see test07_analysis.md for the full discussion.
 *
 * This function looks expensive to the static feature extractor: a
 * 3-level loop nest with a long straight-line body gives it a high
 * instruction_count, max_loop_depth = 3 and a non-trivial memory-op
 * density. The model therefore predicts an expensive, HIGH-tier
 * compilation.
 *
 * In reality every value in the loop body is a compile-time constant.
 * InstCombine / SCCP fold the entire body to a single constant almost
 * immediately, so the expensive loop passes (LoopVectorize, LICM, GVN)
 * never see any real work and the function compiles very fast. The
 * prediction error is large (predicted >> actual).
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

int misleading_complex(int n) {
    int out = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                /* Every line below is loop-invariant and constant: the
                 * operands never depend on i, j, k or any memory. The
                 * compiler folds this whole block to one constant before
                 * the expensive loop optimisations run. */
                int a = 42;
                int b = a * 2;
                int c = b + a * 3;
                int d = c - b + a;
                int e = d * 2 + c;
                int f = e + d - a;
                int g = f * 3 - e;
                int h = g + f + e + d;
                int p = h + g - c;
                int q = p * 2 - h;
                int r = q + p + a;
                int s = r - q + b;
                int t = s * 2 + r;
                int u = t + s - d;
                int v = u * 3 - t;
                int w = v + u + e + f;
                int x = w - v + g;
                int y = x * 2 + w;
                int z = y + x - h;
                out += a + b + c + d + e + f + g + h + p + q
                     + r + s + t + u + v + w + x + y + z;
            }
        }
    }
    return out;
}

int main(void) {
    /* n is small at run time, but the function is compiled generically:
     * the optimiser cannot assume any particular value of n. */
    printf("test07 result=%d\n", misleading_complex(12));
    return 0;
}

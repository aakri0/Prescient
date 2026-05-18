/* t06_phi_heavy.c
 *
 * Complexity pattern: many control-flow merge points. A dozen local
 * variables are each assigned on both sides of several branches, so once
 * mem2reg promotes them to SSA form the merge blocks fill with PHI
 * nodes.
 *
 * Why it is interesting to the model: high PHI density stresses GVN
 * (which tracks value equivalences across PHIs) and register allocation.
 * This is a MEDIUM-tier sample that exercises the phi_* features.
 */

int select_many(int s, int a, int b, int c, int d) {
    int v0, v1, v2, v3, v4, v5, v6, v7, v8, v9;

    if (s > 0) {
        v0 = a;     v1 = b;     v2 = c;     v3 = d;     v4 = a + b;
    } else {
        v0 = b;     v1 = c;     v2 = d;     v3 = a;     v4 = c + d;
    }

    if (s > 5) {
        v5 = v0 + v1;   v6 = v2 - v3;   v7 = v4 * 2;
        v8 = v0 - v2;   v9 = v1 + v3;
    } else {
        v5 = v1;        v6 = v2;        v7 = v3;
        v8 = v4 * 3;    v9 = v0 - v1;
    }

    return v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9;
}

int merge_chain(int n, int seed) {
    int acc = seed;
    int extra = 0;

    if (n > 10)      acc += 5;
    else             acc -= 3;

    if (n % 2 == 0)  extra = acc * 2;
    else             extra = acc + 7;

    if (seed < 0)    acc = extra - acc;
    else             acc = extra + acc;

    return acc + extra;
}

int blend(int x, int y, int flag) {
    int lo, hi, mid;

    if (x < y) { lo = x; hi = y; } else { lo = y; hi = x; }
    mid = (lo + hi) / 2;
    if (flag) mid = hi - lo;

    return lo + hi + mid;
}

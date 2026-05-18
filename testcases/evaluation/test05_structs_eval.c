/* test05_structs_eval.c
 *
 * Held-out evaluation case (NOT used in training): nested-struct data
 * manipulation. Exercises the type-complexity feature.
 *
 * Self-contained runnable program with a deterministic checksum.
 */
#include <stdio.h>

struct Point {
    int x;
    int y;
};

struct Shape {
    struct Point pts[4];
    int weight;
};

static void build(struct Shape *s, int seed) {
    for (int i = 0; i < 4; i++) {
        s->pts[i].x = (seed * (i + 1)) & 0x7F;
        s->pts[i].y = (seed + i * 17) & 0x7F;
    }
    s->weight = (seed & 7) + 1;
}

static long perimeter(const struct Shape *s) {
    long p = 0;
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) & 3;
        int dx = s->pts[i].x - s->pts[j].x;
        int dy = s->pts[i].y - s->pts[j].y;
        p += (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    }
    return p * s->weight;
}

int main(void) {
    struct Shape s;
    long total = 0;
    for (int run = 0; run < 1000000; run++) {
        build(&s, run);
        total += perimeter(&s);
    }
    printf("test05 total=%ld\n", total);
    return 0;
}

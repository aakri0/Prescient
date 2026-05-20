/* t33_geometry.c — Computational geometry functions.
 * Pattern: struct-heavy, moderate arithmetic, some branching.
 */

struct Point { double x, y; };
struct Rect  { double x, y, w, h; };
struct Circle { double cx, cy, r; };

double dist_sq(struct Point a, struct Point b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

double cross2d(struct Point o, struct Point a, struct Point b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

int point_in_rect(struct Point p, struct Rect r) {
    return p.x >= r.x && p.x <= r.x + r.w &&
           p.y >= r.y && p.y <= r.y + r.h;
}

int rects_overlap(struct Rect a, struct Rect b) {
    if (a.x + a.w < b.x || b.x + b.w < a.x) return 0;
    if (a.y + a.h < b.y || b.y + b.h < a.y) return 0;
    return 1;
}

double rect_area(struct Rect r) { return r.w * r.h; }

int point_in_circle(struct Point p, struct Circle c) {
    double dx = p.x - c.cx, dy = p.y - c.cy;
    return dx * dx + dy * dy <= c.r * c.r;
}

int circles_intersect(struct Circle a, struct Circle b) {
    double dx = a.cx - b.cx, dy = a.cy - b.cy;
    double d2 = dx * dx + dy * dy;
    double r_sum = a.r + b.r;
    return d2 <= r_sum * r_sum;
}

/* Convex hull (gift wrapping / Jarvis march) */
int convex_hull(const struct Point *pts, int n, int *hull) {
    if (n < 3) {
        for (int i = 0; i < n; i++) hull[i] = i;
        return n;
    }
    /* Find leftmost point */
    int start = 0;
    for (int i = 1; i < n; i++)
        if (pts[i].x < pts[start].x ||
            (pts[i].x == pts[start].x && pts[i].y < pts[start].y))
            start = i;

    int h = 0, current = start;
    do {
        hull[h++] = current;
        int next = 0;
        for (int i = 1; i < n; i++) {
            if (i == current) continue;
            if (next == current ||
                cross2d(pts[current], pts[next], pts[i]) < 0)
                next = i;
        }
        current = next;
    } while (current != start && h < n);
    return h;
}

double polygon_area(const struct Point *pts, int n) {
    double area = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += pts[i].x * pts[j].y;
        area -= pts[j].x * pts[i].y;
    }
    return area < 0 ? -area / 2 : area / 2;
}

struct Point polygon_centroid(const struct Point *pts, int n) {
    struct Point c = {0, 0};
    for (int i = 0; i < n; i++) { c.x += pts[i].x; c.y += pts[i].y; }
    c.x /= n; c.y /= n;
    return c;
}

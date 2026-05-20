/* t31_physics_sim.c — Simple physics simulation kernels.
 * Pattern: struct arrays, multi-field updates, nested loops (n-body).
 */

struct Particle { double x, y, vx, vy, mass; };

void euler_step(struct Particle *ps, int n, double dt) {
    for (int i = 0; i < n; i++) {
        ps[i].x += ps[i].vx * dt;
        ps[i].y += ps[i].vy * dt;
    }
}

void apply_gravity(struct Particle *ps, int n, double g, double dt) {
    for (int i = 0; i < n; i++)
        ps[i].vy += g * dt;
}

void nbody_forces(struct Particle *ps, int n, double G) {
    for (int i = 0; i < n; i++) {
        double fx = 0, fy = 0;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            double dx = ps[j].x - ps[i].x;
            double dy = ps[j].y - ps[i].y;
            double dist2 = dx * dx + dy * dy + 1e-10;
            double dist = dist2;  /* approx: skip sqrt */
            double force = G * ps[i].mass * ps[j].mass / dist2;
            fx += force * dx / dist;
            fy += force * dy / dist;
        }
        ps[i].vx += fx / ps[i].mass;
        ps[i].vy += fy / ps[i].mass;
    }
}

void bounce_walls(struct Particle *ps, int n, double w, double h) {
    for (int i = 0; i < n; i++) {
        if (ps[i].x < 0) { ps[i].x = -ps[i].x; ps[i].vx = -ps[i].vx * 0.9; }
        if (ps[i].x > w) { ps[i].x = 2*w - ps[i].x; ps[i].vx = -ps[i].vx * 0.9; }
        if (ps[i].y < 0) { ps[i].y = -ps[i].y; ps[i].vy = -ps[i].vy * 0.9; }
        if (ps[i].y > h) { ps[i].y = 2*h - ps[i].y; ps[i].vy = -ps[i].vy * 0.9; }
    }
}

double kinetic_energy(const struct Particle *ps, int n) {
    double ke = 0;
    for (int i = 0; i < n; i++)
        ke += 0.5 * ps[i].mass * (ps[i].vx * ps[i].vx + ps[i].vy * ps[i].vy);
    return ke;
}

void damping(struct Particle *ps, int n, double factor) {
    for (int i = 0; i < n; i++) {
        ps[i].vx *= factor;
        ps[i].vy *= factor;
    }
}

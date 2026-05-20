/* t28_vector_math.c — SIMD-friendly vector math operations.
 * Pattern: simple single-depth loops, uniform memory access, vectorizable.
 */

void vec_add(float *out, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] + b[i];
}

void vec_sub(float *out, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] - b[i];
}

void vec_mul(float *out, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] * b[i];
}

void vec_scale(float *out, const float *a, float s, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] * s;
}

float vec_dot(const float *a, const float *b, int n) {
    float sum = 0;
    for (int i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

float vec_sum(const float *a, int n) {
    float sum = 0;
    for (int i = 0; i < n; i++) sum += a[i];
    return sum;
}

void vec_fma(float *out, const float *a, const float *b, const float *c, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] * b[i] + c[i];
}

void vec_clamp(float *out, const float *a, float lo, float hi, int n) {
    for (int i = 0; i < n; i++) {
        float v = a[i];
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        out[i] = v;
    }
}

void vec_abs(float *out, const float *a, int n) {
    for (int i = 0; i < n; i++)
        out[i] = a[i] < 0 ? -a[i] : a[i];
}

float vec_max(const float *a, int n) {
    float mx = a[0];
    for (int i = 1; i < n; i++)
        if (a[i] > mx) mx = a[i];
    return mx;
}

float vec_min(const float *a, int n) {
    float mn = a[0];
    for (int i = 1; i < n; i++)
        if (a[i] < mn) mn = a[i];
    return mn;
}

void vec_saxpy(float *y, float a, const float *x, int n) {
    for (int i = 0; i < n; i++) y[i] += a * x[i];
}

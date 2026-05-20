/* t17_numerical.c — Numerical methods.
 * Pattern: floating-point loops, convergence checks, moderate depth.
 */

double fabs_d(double x) { return x < 0 ? -x : x; }

double newton_sqrt(double x) {
    if (x <= 0) return 0;
    double guess = x / 2;
    for (int i = 0; i < 50; i++) {
        double next = (guess + x / guess) / 2;
        if (fabs_d(next - guess) < 1e-12) break;
        guess = next;
    }
    return guess;
}

double polynomial_eval(const double *coeffs, int degree, double x) {
    double result = coeffs[degree];
    for (int i = degree - 1; i >= 0; i--)
        result = result * x + coeffs[i];
    return result;
}

double trapezoidal_integrate(const double *y, int n, double dx) {
    double sum = (y[0] + y[n - 1]) / 2;
    for (int i = 1; i < n - 1; i++) sum += y[i];
    return sum * dx;
}

void gauss_eliminate(double *mat, int n) {
    for (int col = 0; col < n; col++) {
        /* Find pivot */
        int pivot = col;
        for (int row = col + 1; row < n; row++)
            if (fabs_d(mat[row * n + col]) > fabs_d(mat[pivot * n + col]))
                pivot = row;
        /* Swap rows */
        if (pivot != col) {
            for (int j = 0; j < n; j++) {
                double t = mat[col * n + j];
                mat[col * n + j] = mat[pivot * n + j];
                mat[pivot * n + j] = t;
            }
        }
        /* Eliminate */
        double diag = mat[col * n + col];
        if (fabs_d(diag) < 1e-15) continue;
        for (int row = col + 1; row < n; row++) {
            double factor = mat[row * n + col] / diag;
            for (int j = col; j < n; j++)
                mat[row * n + j] -= factor * mat[col * n + j];
        }
    }
}

double dot_product(const double *a, const double *b, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

void vec_normalize(double *v, int n) {
    double norm = 0;
    for (int i = 0; i < n; i++) norm += v[i] * v[i];
    norm = newton_sqrt(norm);
    if (norm > 1e-15)
        for (int i = 0; i < n; i++) v[i] /= norm;
}

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

/* t39_statistics.c — Statistical computations.
 * Pattern: multi-pass algorithms, accumulation loops, floating-point.
 */

double stat_mean(const double *data, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) sum += data[i];
    return sum / n;
}

double stat_variance(const double *data, int n) {
    double mean = stat_mean(data, n);
    double var = 0;
    for (int i = 0; i < n; i++) {
        double diff = data[i] - mean;
        var += diff * diff;
    }
    return var / n;
}

double stat_covariance(const double *x, const double *y, int n) {
    double mx = stat_mean(x, n), my = stat_mean(y, n);
    double cov = 0;
    for (int i = 0; i < n; i++)
        cov += (x[i] - mx) * (y[i] - my);
    return cov / n;
}

double stat_median_sorted(const double *sorted, int n) {
    if (n % 2 == 1) return sorted[n / 2];
    return (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
}

int stat_mode(const int *data, int n) {
    int best_val = data[0], best_count = 1;
    for (int i = 0; i < n; i++) {
        int count = 0;
        for (int j = 0; j < n; j++)
            if (data[j] == data[i]) count++;
        if (count > best_count) {
            best_count = count;
            best_val = data[i];
        }
    }
    return best_val;
}

double stat_percentile(const double *sorted, int n, double p) {
    double rank = p * (n - 1) / 100.0;
    int lo = (int)rank;
    int hi = lo + 1;
    if (hi >= n) return sorted[n - 1];
    double frac = rank - lo;
    return sorted[lo] * (1 - frac) + sorted[hi] * frac;
}

void stat_zscore(double *out, const double *data, int n) {
    double mean = stat_mean(data, n);
    double var = stat_variance(data, n);
    double std = var;  /* skip sqrt for simplicity */
    if (std < 1e-15) std = 1;
    for (int i = 0; i < n; i++)
        out[i] = (data[i] - mean) / std;
}

double stat_correlation(const double *x, const double *y, int n) {
    double cov = stat_covariance(x, y, n);
    double vx = stat_variance(x, n);
    double vy = stat_variance(y, n);
    double denom = vx * vy;  /* skip sqrt */
    if (denom < 1e-15) return 0;
    return cov / denom;
}

void linear_regression(const double *x, const double *y, int n,
                       double *slope, double *intercept) {
    double mx = stat_mean(x, n), my = stat_mean(y, n);
    double num = 0, den = 0;
    for (int i = 0; i < n; i++) {
        double dx = x[i] - mx;
        num += dx * (y[i] - my);
        den += dx * dx;
    }
    *slope = den > 1e-15 ? num / den : 0;
    *intercept = my - (*slope) * mx;
}

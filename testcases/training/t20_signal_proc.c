/* t20_signal_proc.c — Signal processing operations.
 * Pattern: inner-product loops, sliding windows, SIMD-friendly patterns.
 */

void fir_filter(double *out, const double *in, int n,
                const double *taps, int ntaps) {
    for (int i = 0; i < n; i++) {
        double acc = 0;
        for (int j = 0; j < ntaps; j++) {
            int idx = i - j;
            if (idx >= 0) acc += in[idx] * taps[j];
        }
        out[i] = acc;
    }
}

void moving_average(double *out, const double *in, int n, int window) {
    for (int i = 0; i < n; i++) {
        double sum = 0;
        int count = 0;
        for (int j = i - window / 2; j <= i + window / 2; j++) {
            if (j >= 0 && j < n) { sum += in[j]; count++; }
        }
        out[i] = sum / count;
    }
}

double rms(const double *signal, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) sum += signal[i] * signal[i];
    return sum / n;  /* sqrt omitted to avoid libm */
}

void cross_correlate(double *out, const double *a, const double *b, int n) {
    for (int lag = 0; lag < n; lag++) {
        double sum = 0;
        for (int i = 0; i < n - lag; i++)
            sum += a[i] * b[i + lag];
        out[lag] = sum;
    }
}

void convolve(double *out, const double *a, int na,
              const double *b, int nb) {
    int nc = na + nb - 1;
    for (int i = 0; i < nc; i++) {
        out[i] = 0;
        for (int j = 0; j < na; j++) {
            int k = i - j;
            if (k >= 0 && k < nb)
                out[i] += a[j] * b[k];
        }
    }
}

void downsample(double *out, const double *in, int n, int factor) {
    for (int i = 0; i < n / factor; i++)
        out[i] = in[i * factor];
}

void threshold(double *out, const double *in, int n, double thresh) {
    for (int i = 0; i < n; i++)
        out[i] = (in[i] > thresh) ? in[i] : 0;
}

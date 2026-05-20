/* t13_matrix_ops.c — Matrix operations.
 * Pattern: nested loops (depth 2-3), heavy memory traffic, SIMD-friendly.
 */

#define N 64

void mat_add(int dst[N][N], const int a[N][N], const int b[N][N]) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            dst[i][j] = a[i][j] + b[i][j];
}

void mat_mul(int dst[N][N], const int a[N][N], const int b[N][N]) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            int sum = 0;
            for (int k = 0; k < N; k++)
                sum += a[i][k] * b[k][j];
            dst[i][j] = sum;
        }
}

void mat_transpose(int dst[N][N], const int src[N][N]) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            dst[j][i] = src[i][j];
}

int mat_trace(const int m[N][N]) {
    int sum = 0;
    for (int i = 0; i < N; i++) sum += m[i][i];
    return sum;
}

void mat_scalar_mul(int m[N][N], int scalar) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            m[i][j] *= scalar;
}

int mat_max_element(const int m[N][N]) {
    int mx = m[0][0];
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (m[i][j] > mx) mx = m[i][j];
    return mx;
}

void mat_row_sum(int out[N], const int m[N][N]) {
    for (int i = 0; i < N; i++) {
        out[i] = 0;
        for (int j = 0; j < N; j++)
            out[i] += m[i][j];
    }
}

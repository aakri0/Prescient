/* t23_compression.c — Compression-like algorithms.
 * Pattern: byte-level loops, counting, run detection, moderate branching.
 */

int rle_encode(const unsigned char *in, int n, unsigned char *out) {
    int pos = 0;
    int i = 0;
    while (i < n) {
        unsigned char val = in[i];
        int run = 1;
        while (i + run < n && in[i + run] == val && run < 255) run++;
        out[pos++] = (unsigned char)run;
        out[pos++] = val;
        i += run;
    }
    return pos;
}

int rle_decode(const unsigned char *in, int n, unsigned char *out) {
    int pos = 0;
    for (int i = 0; i + 1 < n; i += 2) {
        int run = in[i];
        unsigned char val = in[i + 1];
        for (int j = 0; j < run; j++) out[pos++] = val;
    }
    return pos;
}

void build_frequency_table(const unsigned char *data, int n, int freq[256]) {
    for (int i = 0; i < 256; i++) freq[i] = 0;
    for (int i = 0; i < n; i++) freq[data[i]]++;
}

int count_unique_bytes(const unsigned char *data, int n) {
    int seen[256];
    for (int i = 0; i < 256; i++) seen[i] = 0;
    for (int i = 0; i < n; i++) seen[data[i]] = 1;
    int count = 0;
    for (int i = 0; i < 256; i++) count += seen[i];
    return count;
}

double entropy_estimate(const int freq[256], int total) {
    double ent = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        double p = (double)freq[i] / total;
        /* Approximate -p*log2(p) without libm: use p*(1-p) as proxy */
        ent += p * (1.0 - p);
    }
    return ent;
}

int lz_find_match(const unsigned char *buf, int pos, int window_size,
                  int *match_off, int *match_len) {
    int best_len = 0, best_off = 0;
    int start = pos - window_size;
    if (start < 0) start = 0;
    for (int i = start; i < pos; i++) {
        int len = 0;
        while (pos + len < pos + 258 && buf[i + len] == buf[pos + len])
            len++;
        if (len > best_len) {
            best_len = len;
            best_off = pos - i;
        }
    }
    *match_off = best_off;
    *match_len = best_len;
    return best_len >= 3;
}

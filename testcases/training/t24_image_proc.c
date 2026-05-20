/* t24_image_proc.c — Image processing kernels.
 * Pattern: 2D nested loops, stencil operations, heavy memory traffic.
 */

#define IMG_W 256
#define IMG_H 256

void grayscale(unsigned char *out, const unsigned char *rgb, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 3;
            out[y * w + x] = (unsigned char)(
                (77 * rgb[idx] + 150 * rgb[idx+1] + 29 * rgb[idx+2]) >> 8);
        }
}

void box_blur_3x3(unsigned char *out, const unsigned char *in, int w, int h) {
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++) {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    sum += in[(y + dy) * w + (x + dx)];
            out[y * w + x] = (unsigned char)(sum / 9);
        }
}

void sobel_x(int *out, const unsigned char *in, int w, int h) {
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++) {
            int gx = -in[(y-1)*w+(x-1)] + in[(y-1)*w+(x+1)]
                     -2*in[y*w+(x-1)]   + 2*in[y*w+(x+1)]
                     -in[(y+1)*w+(x-1)] + in[(y+1)*w+(x+1)];
            out[y * w + x] = gx;
        }
}

void threshold_image(unsigned char *img, int w, int h, unsigned char thresh) {
    for (int i = 0; i < w * h; i++)
        img[i] = img[i] > thresh ? 255 : 0;
}

void invert_image(unsigned char *img, int w, int h) {
    for (int i = 0; i < w * h; i++)
        img[i] = 255 - img[i];
}

int histogram(const unsigned char *img, int w, int h, int bins[256]) {
    for (int i = 0; i < 256; i++) bins[i] = 0;
    for (int i = 0; i < w * h; i++) bins[img[i]]++;
    int peak = 0;
    for (int i = 0; i < 256; i++)
        if (bins[i] > bins[peak]) peak = i;
    return peak;
}

void dilate_3x3(unsigned char *out, const unsigned char *in, int w, int h) {
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++) {
            unsigned char mx = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    unsigned char v = in[(y+dy)*w+(x+dx)];
                    if (v > mx) mx = v;
                }
            out[y * w + x] = mx;
        }
}

void erode_3x3(unsigned char *out, const unsigned char *in, int w, int h) {
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++) {
            unsigned char mn = 255;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    unsigned char v = in[(y+dy)*w+(x+dx)];
                    if (v < mn) mn = v;
                }
            out[y * w + x] = mn;
        }
}

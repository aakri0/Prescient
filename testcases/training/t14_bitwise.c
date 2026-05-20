/* t14_bitwise.c — Bit manipulation functions.
 * Pattern: many small single-BB functions, minimal loops, no memory.
 */

int popcount(unsigned int x) {
    int count = 0;
    while (x) { count += x & 1; x >>= 1; }
    return count;
}

unsigned int reverse_bits(unsigned int x) {
    unsigned int r = 0;
    for (int i = 0; i < 32; i++) {
        r = (r << 1) | (x & 1);
        x >>= 1;
    }
    return r;
}

int parity(unsigned int x) {
    x ^= x >> 16; x ^= x >> 8;
    x ^= x >> 4;  x ^= x >> 2;
    x ^= x >> 1;
    return x & 1;
}

int count_leading_zeros(unsigned int x) {
    if (x == 0) return 32;
    int n = 0;
    if ((x & 0xFFFF0000) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF000000) == 0) { n += 8;  x <<= 8; }
    if ((x & 0xF0000000) == 0) { n += 4;  x <<= 4; }
    if ((x & 0xC0000000) == 0) { n += 2;  x <<= 2; }
    if ((x & 0x80000000) == 0) { n += 1; }
    return n;
}

unsigned int next_power_of_two(unsigned int x) {
    x--; x |= x >> 1; x |= x >> 2;
    x |= x >> 4; x |= x >> 8; x |= x >> 16;
    return x + 1;
}

int is_power_of_two(unsigned int x) {
    return x && !(x & (x - 1));
}

unsigned int rotate_left(unsigned int x, int n) {
    return (x << n) | (x >> (32 - n));
}

unsigned int rotate_right(unsigned int x, int n) {
    return (x >> n) | (x << (32 - n));
}

unsigned int interleave_bits(unsigned int x, unsigned int y) {
    unsigned int result = 0;
    for (int i = 0; i < 16; i++)
        result |= ((x & (1u << i)) << i) | ((y & (1u << i)) << (i + 1));
    return result;
}

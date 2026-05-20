/* t18_crypto_hash.c — Crypto-like hash and cipher operations.
 * Pattern: bitwise ops in loops, unrolled rounds, constant mixing.
 */

static unsigned int rotl32(unsigned int x, int n) {
    return (x << n) | (x >> (32 - n));
}

unsigned int fnv1a_hash(const unsigned char *data, int len) {
    unsigned int h = 0x811c9dc5;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x01000193;
    }
    return h;
}

unsigned int djb2_hash(const unsigned char *data, int len) {
    unsigned int h = 5381;
    for (int i = 0; i < len; i++)
        h = ((h << 5) + h) + data[i];
    return h;
}

unsigned int murmur_mix(unsigned int h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

void xor_cipher(unsigned char *buf, int len, unsigned int key) {
    for (int i = 0; i < len; i++) {
        key = key * 1103515245 + 12345;
        buf[i] ^= (key >> 16) & 0xFF;
    }
}

void sbox_transform(unsigned char *buf, int len, const unsigned char sbox[256]) {
    for (int i = 0; i < len; i++)
        buf[i] = sbox[buf[i]];
}

unsigned int crc32_naive(const unsigned char *data, int len) {
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

void feistel_round(unsigned int *left, unsigned int *right, unsigned int key) {
    unsigned int f = rotl32(*right ^ key, 7) + (*right >> 3);
    unsigned int new_right = *left ^ f;
    *left = *right;
    *right = new_right;
}

void feistel_encrypt(unsigned int *block, const unsigned int *keys, int rounds) {
    unsigned int left = block[0], right = block[1];
    for (int i = 0; i < rounds; i++)
        feistel_round(&left, &right, keys[i]);
    block[0] = left;
    block[1] = right;
}

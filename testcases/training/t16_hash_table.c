/* t16_hash_table.c — Hash table with chaining.
 * Pattern: pointer chasing, moderate branches, array indexing, mixed ops.
 */

#define HT_SIZE 256

struct Entry { int key; int value; struct Entry *next; };
struct HashTable { struct Entry *buckets[HT_SIZE]; };

static unsigned int hash_int(int key) {
    unsigned int h = (unsigned int)key;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h & (HT_SIZE - 1);
}

int ht_get(struct HashTable *ht, int key) {
    unsigned int idx = hash_int(key);
    struct Entry *e = ht->buckets[idx];
    while (e) {
        if (e->key == key) return e->value;
        e = e->next;
    }
    return -1;
}

int ht_contains(struct HashTable *ht, int key) {
    unsigned int idx = hash_int(key);
    struct Entry *e = ht->buckets[idx];
    while (e) {
        if (e->key == key) return 1;
        e = e->next;
    }
    return 0;
}

int ht_count(struct HashTable *ht) {
    int count = 0;
    for (int i = 0; i < HT_SIZE; i++) {
        struct Entry *e = ht->buckets[i];
        while (e) { count++; e = e->next; }
    }
    return count;
}

int ht_sum_values(struct HashTable *ht) {
    int sum = 0;
    for (int i = 0; i < HT_SIZE; i++) {
        struct Entry *e = ht->buckets[i];
        while (e) { sum += e->value; e = e->next; }
    }
    return sum;
}

int ht_max_chain_length(struct HashTable *ht) {
    int mx = 0;
    for (int i = 0; i < HT_SIZE; i++) {
        int len = 0;
        struct Entry *e = ht->buckets[i];
        while (e) { len++; e = e->next; }
        if (len > mx) mx = len;
    }
    return mx;
}

int ht_empty_buckets(struct HashTable *ht) {
    int count = 0;
    for (int i = 0; i < HT_SIZE; i++)
        if (!ht->buckets[i]) count++;
    return count;
}

/* t30_arena_alloc.c — Memory allocator patterns.
 * Pattern: pointer arithmetic, alignment, free-list walking.
 */

struct Arena {
    char *base;
    int size;
    int offset;
};

void arena_init(struct Arena *a, char *buf, int size) {
    a->base = buf;
    a->size = size;
    a->offset = 0;
}

void *arena_alloc(struct Arena *a, int nbytes) {
    /* Align to 8 bytes */
    int aligned = (a->offset + 7) & ~7;
    if (aligned + nbytes > a->size) return 0;
    void *ptr = a->base + aligned;
    a->offset = aligned + nbytes;
    return ptr;
}

void arena_reset(struct Arena *a) {
    a->offset = 0;
}

int arena_remaining(struct Arena *a) {
    return a->size - a->offset;
}

/* Simple bump allocator with per-block headers */
struct BlockHeader { int size; int free; struct BlockHeader *next; };

int pool_count_free(struct BlockHeader *head) {
    int count = 0;
    while (head) {
        if (head->free) count++;
        head = head->next;
    }
    return count;
}

int pool_total_free(struct BlockHeader *head) {
    int total = 0;
    while (head) {
        if (head->free) total += head->size;
        head = head->next;
    }
    return total;
}

int pool_largest_free(struct BlockHeader *head) {
    int largest = 0;
    while (head) {
        if (head->free && head->size > largest)
            largest = head->size;
        head = head->next;
    }
    return largest;
}

int pool_fragmentation_score(struct BlockHeader *head) {
    int free_blocks = 0, total_free = 0, largest = 0;
    while (head) {
        if (head->free) {
            free_blocks++;
            total_free += head->size;
            if (head->size > largest) largest = head->size;
        }
        head = head->next;
    }
    if (total_free == 0) return 0;
    /* Score: 100 means all free space in one block (no fragmentation) */
    return (largest * 100) / total_free;
}

void pool_coalesce(struct BlockHeader *head) {
    while (head && head->next) {
        if (head->free && head->next->free) {
            head->size += head->next->size + (int)sizeof(struct BlockHeader);
            head->next = head->next->next;
        } else {
            head = head->next;
        }
    }
}

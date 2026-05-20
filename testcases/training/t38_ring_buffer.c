/* t38_ring_buffer.c — Ring buffer and queue operations.
 * Pattern: modular arithmetic, pointer wrapping, moderate branching.
 */

#define RING_CAP 1024

struct RingBuffer {
    int data[RING_CAP];
    int head, tail, count;
};

void ring_init(struct RingBuffer *rb) {
    rb->head = 0; rb->tail = 0; rb->count = 0;
}

int ring_push(struct RingBuffer *rb, int val) {
    if (rb->count >= RING_CAP) return 0;
    rb->data[rb->tail] = val;
    rb->tail = (rb->tail + 1) % RING_CAP;
    rb->count++;
    return 1;
}

int ring_pop(struct RingBuffer *rb, int *out) {
    if (rb->count <= 0) return 0;
    *out = rb->data[rb->head];
    rb->head = (rb->head + 1) % RING_CAP;
    rb->count--;
    return 1;
}

int ring_peek(const struct RingBuffer *rb) {
    return rb->count > 0 ? rb->data[rb->head] : -1;
}

int ring_full(const struct RingBuffer *rb) {
    return rb->count >= RING_CAP;
}

int ring_empty(const struct RingBuffer *rb) {
    return rb->count == 0;
}

int ring_sum(const struct RingBuffer *rb) {
    int sum = 0, idx = rb->head;
    for (int i = 0; i < rb->count; i++) {
        sum += rb->data[idx];
        idx = (idx + 1) % RING_CAP;
    }
    return sum;
}

int ring_max(const struct RingBuffer *rb) {
    if (rb->count == 0) return 0;
    int mx = rb->data[rb->head], idx = rb->head;
    for (int i = 1; i < rb->count; i++) {
        idx = (idx + 1) % RING_CAP;
        if (rb->data[idx] > mx) mx = rb->data[idx];
    }
    return mx;
}

int ring_contains(const struct RingBuffer *rb, int val) {
    int idx = rb->head;
    for (int i = 0; i < rb->count; i++) {
        if (rb->data[idx] == val) return 1;
        idx = (idx + 1) % RING_CAP;
    }
    return 0;
}

void ring_drain_to_array(struct RingBuffer *rb, int *out) {
    int idx = rb->head;
    for (int i = 0; i < rb->count; i++) {
        out[i] = rb->data[idx];
        idx = (idx + 1) % RING_CAP;
    }
    rb->head = 0; rb->tail = 0; rb->count = 0;
}

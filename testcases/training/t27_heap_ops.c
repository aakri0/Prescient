/* t27_heap_ops.c — Binary heap (min-heap) operations.
 * Pattern: while-loops with divide-by-2, conditional swaps, log-depth.
 */

static void heap_swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

void sift_up(int *heap, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (heap[parent] <= heap[idx]) break;
        heap_swap(&heap[parent], &heap[idx]);
        idx = parent;
    }
}

void sift_down(int *heap, int size, int idx) {
    while (2 * idx + 1 < size) {
        int child = 2 * idx + 1;
        if (child + 1 < size && heap[child + 1] < heap[child]) child++;
        if (heap[idx] <= heap[child]) break;
        heap_swap(&heap[idx], &heap[child]);
        idx = child;
    }
}

void heap_insert(int *heap, int *size, int val) {
    heap[*size] = val;
    sift_up(heap, *size);
    (*size)++;
}

int heap_extract_min(int *heap, int *size) {
    int min = heap[0];
    (*size)--;
    heap[0] = heap[*size];
    sift_down(heap, *size, 0);
    return min;
}

void build_heap(int *arr, int n) {
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(arr, n, i);
}

void heapsort(int *arr, int n) {
    build_heap(arr, n);
    for (int i = n - 1; i > 0; i--) {
        heap_swap(&arr[0], &arr[i]);
        /* max-heap sift_down variant inline */
        int size = i, idx = 0;
        while (2 * idx + 1 < size) {
            int child = 2 * idx + 1;
            if (child + 1 < size && arr[child + 1] > arr[child]) child++;
            if (arr[idx] >= arr[child]) break;
            heap_swap(&arr[idx], &arr[child]);
            idx = child;
        }
    }
}

int heap_peek(const int *heap) {
    return heap[0];
}

int is_min_heap(const int *arr, int n) {
    for (int i = 0; i < n / 2; i++) {
        int left = 2 * i + 1, right = 2 * i + 2;
        if (left < n && arr[left] < arr[i]) return 0;
        if (right < n && arr[right] < arr[i]) return 0;
    }
    return 1;
}

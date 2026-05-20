/* t26_array_search.c — Array search and manipulation algorithms.
 * Pattern: single loops, varying branch density, some use binary search.
 */

int linear_search(const int *arr, int n, int key) {
    for (int i = 0; i < n; i++)
        if (arr[i] == key) return i;
    return -1;
}

int binary_search(const int *arr, int n, int key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == key) return mid;
        if (arr[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

int lower_bound(const int *arr, int n, int key) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

int count_occurrences(const int *arr, int n, int key) {
    int count = 0;
    for (int i = 0; i < n; i++)
        if (arr[i] == key) count++;
    return count;
}

void rotate_left(int *arr, int n, int k) {
    k %= n;
    /* Three reverses */
    for (int i = 0, j = k - 1; i < j; i++, j--) {
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
    for (int i = k, j = n - 1; i < j; i++, j--) {
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
    for (int i = 0, j = n - 1; i < j; i++, j--) {
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}

int remove_duplicates_sorted(int *arr, int n) {
    if (n <= 1) return n;
    int write = 1;
    for (int i = 1; i < n; i++)
        if (arr[i] != arr[i - 1])
            arr[write++] = arr[i];
    return write;
}

void merge_sorted(int *out, const int *a, int na, const int *b, int nb) {
    int i = 0, j = 0, k = 0;
    while (i < na && j < nb)
        out[k++] = (a[i] <= b[j]) ? a[i++] : b[j++];
    while (i < na) out[k++] = a[i++];
    while (j < nb) out[k++] = b[j++];
}

int kth_smallest(int *arr, int n, int k) {
    /* Simple selection via partition */
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int pivot = arr[hi], i = lo;
        for (int j = lo; j < hi; j++)
            if (arr[j] <= pivot) {
                int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
                i++;
            }
        int t = arr[i]; arr[i] = arr[hi]; arr[hi] = t;
        if (i == k) return arr[i];
        if (i < k) lo = i + 1;
        else hi = i - 1;
    }
    return arr[lo];
}

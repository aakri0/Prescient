/* t08_recursive.c
 *
 * Complexity pattern: recursive functions — naive Fibonacci, factorial
 * and a recursive binary-tree-style sum over an array.
 *
 * Why it is interesting to the model: recursion makes the inliner work
 * hard (recursive calls cannot be fully inlined) and produces call-heavy
 * IR with non-trivial call-site counts. This is a MEDIUM-tier sample.
 */

int fib(int n) {
    if (n < 2)
        return n;
    return fib(n - 1) + fib(n - 2);
}

long factorial(int n) {
    if (n <= 1)
        return 1;
    return (long)n * factorial(n - 1);
}

int tree_sum(const int *values, int lo, int hi) {
    if (lo > hi)
        return 0;
    if (lo == hi)
        return values[lo];
    int mid = lo + (hi - lo) / 2;
    int left = tree_sum(values, lo, mid);
    int right = tree_sum(values, mid + 1, hi);
    return left + right;
}

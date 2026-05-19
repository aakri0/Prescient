/* sample_math.c
 *
 * A self-contained test program for Prescient: a spread of basic math
 * functions (leaf arithmetic, loops, recursion, branches, array ops) plus
 * print helpers. Use it to exercise the feature extractor and predictor:
 *
 *   ./run.sh extract testcases/sample_math.c
 *   ./run.sh predict testcases/sample_math.c
 *
 * Or in Docker:
 *   docker compose run --rm prescient extract testcases/sample_math.c
 *
 * Compiles cleanly with: clang-17 -O0 -Wall -Wextra -std=c99
 */
#include <stdio.h>

/* --- leaf arithmetic: a single basic block, lowest complexity --- */
int add(int a, int b)      { return a + b; }
int subtract(int a, int b) { return a - b; }
int multiply(int a, int b) { return a * b; }

/* --- loop: integer power by repeated multiplication --- */
long power(int base, int exp) {
    long result = 1;
    for (int i = 0; i < exp; i++)
        result *= base;
    return result;
}

/* --- recursion: factorial --- */
long factorial(int n) {
    if (n <= 1)
        return 1;
    return (long)n * factorial(n - 1);
}

/* --- recursion: naive Fibonacci (two recursive calls per level) --- */
int fibonacci(int n) {
    if (n < 2)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

/* --- recursion + branch: greatest common divisor --- */
int gcd(int a, int b) {
    if (b == 0)
        return a;
    return gcd(b, a % b);
}

/* --- loop + branches: primality test --- */
int is_prime(int n) {
    if (n < 2)
        return 0;
    for (int d = 2; d * d <= n; d++) {
        if (n % d == 0)
            return 0;
    }
    return 1;
}

/* --- loop over an array --- */
long sum_array(const int *values, int count) {
    long total = 0;
    for (int i = 0; i < count; i++)
        total += values[i];
    return total;
}

/* --- loop + division: arithmetic mean --- */
double average(const int *values, int count) {
    if (count == 0)
        return 0.0;
    return (double)sum_array(values, count) / count;
}

/* --- print helpers --- */
void print_header(const char *title) {
    printf("==== %s ====\n", title);
}

void print_int_result(const char *label, long value) {
    printf("  %-18s = %ld\n", label, value);
}

void print_double_result(const char *label, double value) {
    printf("  %-18s = %.2f\n", label, value);
}

int main(void) {
    int data[8] = { 4, 8, 15, 16, 23, 42, 1, 91 };

    print_header("Basic arithmetic");
    print_int_result("add(6, 7)", add(6, 7));
    print_int_result("subtract(20, 8)", subtract(20, 8));
    print_int_result("multiply(6, 9)", multiply(6, 9));

    print_header("Loops and recursion");
    print_int_result("power(2, 10)", power(2, 10));
    print_int_result("factorial(8)", factorial(8));
    print_int_result("fibonacci(15)", fibonacci(15));
    print_int_result("gcd(1071, 462)", gcd(1071, 462));
    print_int_result("is_prime(97)", is_prime(97));

    print_header("Array operations");
    print_int_result("sum_array", sum_array(data, 8));
    print_double_result("average", average(data, 8));

    return 0;
}

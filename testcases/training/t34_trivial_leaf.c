/* t34_trivial_leaf.c — More trivial leaf functions to anchor the low end.
 * Pattern: single basic block, no loops, no branches — pure arithmetic.
 */

int abs_val(int x) { return x < 0 ? -x : x; }
int max2(int a, int b) { return a > b ? a : b; }
int min2(int a, int b) { return a < b ? a : b; }
int clamp_int(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
int sign(int x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }
int square(int x) { return x * x; }
int cube(int x) { return x * x * x; }
int avg2(int a, int b) { return (a + b) / 2; }
int diff(int a, int b) { return a > b ? a - b : b - a; }
int is_even(int x) { return (x & 1) == 0; }
int is_odd(int x)  { return (x & 1) != 0; }
int gcd(int a, int b) { while (b) { int t = b; b = a % b; a = t; } return a; }
int lcm(int a, int b) { return a / gcd(a, b) * b; }
int factorial_iter(int n) {
    int r = 1;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}
int sum_range(int lo, int hi) {
    int s = 0;
    for (int i = lo; i <= hi; i++) s += i;
    return s;
}
int power_int(int base, int exp) {
    int r = 1;
    for (int i = 0; i < exp; i++) r *= base;
    return r;
}
int count_digits(int n) {
    if (n == 0) return 1;
    int c = 0;
    if (n < 0) n = -n;
    while (n) { c++; n /= 10; }
    return c;
}
int reverse_int(int n) {
    int r = 0;
    while (n) { r = r * 10 + n % 10; n /= 10; }
    return r;
}
int is_palindrome(int n) { return n == reverse_int(n); }
int sum_of_digits(int n) {
    int s = 0;
    if (n < 0) n = -n;
    while (n) { s += n % 10; n /= 10; }
    return s;
}

/* t01_simple_leaf.c
 *
 * Complexity pattern: leaf functions — pure integer arithmetic with no
 * loops, no branches and no memory traffic. Each function is a single
 * basic block.
 *
 * Why it is interesting to the model: this is the clean LOW-tier
 * baseline. Every function here has cyclomatic_complexity = 1 and
 * loop_count = 0, so the optimiser does almost no work. It anchors the
 * low end of the feature-to-cost relationship the model learns.
 */

int add_ints(int a, int b) {
    return a + b;
}

int subtract_ints(int a, int b) {
    return a - b;
}

int multiply_ints(int a, int b) {
    return a * b;
}

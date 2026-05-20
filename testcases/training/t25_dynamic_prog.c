/* t25_dynamic_prog.c — Dynamic programming algorithms.
 * Pattern: 2D table fills, nested loops, conditional updates.
 */

#define DP_MAX 256

int fibonacci_iter(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int c = a + b; a = b; b = c;
    }
    return b;
}

int lcs_length(const char *a, int m, const char *b, int n, int dp[DP_MAX][DP_MAX]) {
    for (int i = 0; i <= m; i++) dp[i][0] = 0;
    for (int j = 0; j <= n; j++) dp[0][j] = 0;
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++) {
            if (a[i-1] == b[j-1])
                dp[i][j] = dp[i-1][j-1] + 1;
            else
                dp[i][j] = dp[i-1][j] > dp[i][j-1] ? dp[i-1][j] : dp[i][j-1];
        }
    return dp[m][n];
}

int edit_distance(const char *a, int m, const char *b, int n, int dp[DP_MAX][DP_MAX]) {
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++) {
            int cost = (a[i-1] != b[j-1]) ? 1 : 0;
            int del = dp[i-1][j] + 1;
            int ins = dp[i][j-1] + 1;
            int sub = dp[i-1][j-1] + cost;
            dp[i][j] = del;
            if (ins < dp[i][j]) dp[i][j] = ins;
            if (sub < dp[i][j]) dp[i][j] = sub;
        }
    return dp[m][n];
}

int knapsack_01(const int *weights, const int *values, int n, int capacity,
                int dp[DP_MAX]) {
    for (int w = 0; w <= capacity; w++) dp[w] = 0;
    for (int i = 0; i < n; i++)
        for (int w = capacity; w >= weights[i]; w--) {
            int with = dp[w - weights[i]] + values[i];
            if (with > dp[w]) dp[w] = with;
        }
    return dp[capacity];
}

int coin_change(const int *coins, int n, int amount, int dp[DP_MAX]) {
    dp[0] = 0;
    for (int i = 1; i <= amount; i++) dp[i] = amount + 1;
    for (int i = 1; i <= amount; i++)
        for (int j = 0; j < n; j++)
            if (coins[j] <= i && dp[i - coins[j]] + 1 < dp[i])
                dp[i] = dp[i - coins[j]] + 1;
    return dp[amount] > amount ? -1 : dp[amount];
}

int max_subarray(const int *arr, int n) {
    int max_so_far = arr[0], current = arr[0];
    for (int i = 1; i < n; i++) {
        current = arr[i] > current + arr[i] ? arr[i] : current + arr[i];
        if (current > max_so_far) max_so_far = current;
    }
    return max_so_far;
}

int longest_increasing_subseq(const int *arr, int n) {
    int dp_arr[DP_MAX];
    for (int i = 0; i < n; i++) dp_arr[i] = 1;
    for (int i = 1; i < n; i++)
        for (int j = 0; j < i; j++)
            if (arr[j] < arr[i] && dp_arr[j] + 1 > dp_arr[i])
                dp_arr[i] = dp_arr[j] + 1;
    int mx = 0;
    for (int i = 0; i < n; i++)
        if (dp_arr[i] > mx) mx = dp_arr[i];
    return mx;
}

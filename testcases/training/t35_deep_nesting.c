/* t35_deep_nesting.c — Deeply nested control flow.
 * Pattern: high cyclomatic complexity, many basic blocks, deep nesting.
 */

int deep_if_chain(int a, int b, int c, int d) {
    int result = 0;
    if (a > 0) {
        if (b > 0) {
            if (c > 0) {
                if (d > 0) result = a + b + c + d;
                else if (d > -10) result = a + b + c;
                else result = a + b;
            } else if (c > -10) {
                if (d > 0) result = a + b - c + d;
                else result = a + b - c;
            } else {
                result = a + b;
            }
        } else if (b > -10) {
            if (c > 0) result = a - b + c;
            else result = a - b;
        } else {
            result = a;
        }
    } else if (a > -10) {
        if (b > 0) result = b + c;
        else result = c + d;
    } else {
        result = d;
    }
    return result;
}

int nested_switch(int category, int subcategory, int option) {
    int result = 0;
    switch (category) {
    case 0:
        switch (subcategory) {
        case 0: result = option * 2; break;
        case 1: result = option * 3; break;
        case 2: result = option + 10; break;
        default: result = option; break;
        }
        break;
    case 1:
        switch (subcategory) {
        case 0: result = option - 5; break;
        case 1: result = option | 0xFF; break;
        case 2: result = option & 0x0F; break;
        default: result = -option; break;
        }
        break;
    case 2:
        switch (subcategory) {
        case 0: result = option << 2; break;
        case 1: result = option >> 1; break;
        default: result = option ^ 0xAA; break;
        }
        break;
    default:
        result = category + subcategory + option;
        break;
    }
    return result;
}

int quad_nested_loop(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++)
                for (int l = 0; l < n; l++)
                    sum += (i ^ j) + (k ^ l);
    return sum;
}

int complex_conditional_loop(const int *data, int n) {
    int a = 0, b = 0, c = 0, d = 0;
    for (int i = 0; i < n; i++) {
        int v = data[i];
        if (v > 100) {
            if (v > 200) a += v;
            else if (v > 150) b += v;
            else a += v / 2;
        } else if (v > 50) {
            if (v % 2 == 0) c += v;
            else d += v;
        } else if (v > 0) {
            a++; b++;
        } else if (v > -50) {
            c--; d--;
        } else {
            a -= v; c += v;
        }
    }
    return a + b * 2 + c * 3 + d * 4;
}

int multi_condition_loop(const int *arr, int n) {
    int r = 0;
    for (int i = 0; i < n; i++) {
        int v = arr[i];
        if      (v < -1000) r += 1;
        else if (v < -500)  r += 2;
        else if (v < -100)  r += 3;
        else if (v < -50)   r += 4;
        else if (v < -10)   r += 5;
        else if (v < 0)     r += 6;
        else if (v == 0)    r += 7;
        else if (v < 10)    r += 8;
        else if (v < 50)    r += 9;
        else if (v < 100)   r += 10;
        else if (v < 500)   r += 11;
        else if (v < 1000)  r += 12;
        else                r += 13;
    }
    return r;
}

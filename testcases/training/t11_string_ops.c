/* t11_string_ops.c — String manipulation functions.
 * Pattern: moderate branches, sequential memory access, varying loop counts.
 */

int my_strlen(const char *s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

void my_strcpy(char *dst, const char *src) {
    int i = 0;
    while ((dst[i] = src[i]) != '\0') i++;
}

int my_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int count_char(const char *s, char c) {
    int count = 0;
    for (int i = 0; s[i]; i++)
        if (s[i] == c) count++;
    return count;
}

void reverse_string(char *s, int len) {
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        char t = s[i]; s[i] = s[j]; s[j] = t;
    }
}

int find_substring(const char *haystack, const char *needle) {
    for (int i = 0; haystack[i]; i++) {
        int j = 0;
        while (needle[j] && haystack[i + j] == needle[j]) j++;
        if (!needle[j]) return i;
    }
    return -1;
}

void to_uppercase(char *s) {
    for (int i = 0; s[i]; i++)
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] -= 32;
}

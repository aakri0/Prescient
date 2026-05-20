/* t37_regex_like.c — Pattern matching and text processing.
 * Pattern: character-level loops, backtracking, state machines.
 */

/* Forward declarations for mutual recursion */
int match_here(const char *pattern, const char *text);
int match_star(char p, const char *pattern, const char *text);

int match_char(char pattern, char text) {
    if (pattern == '.') return text != '\0';
    return pattern == text;
}

int match_star(char p, const char *pattern, const char *text) {
    do {
        if (match_here(pattern, text)) return 1;
    } while (*text != '\0' && match_char(p, *text++));
    return 0;
}

int match_here(const char *pattern, const char *text) {
    if (pattern[0] == '\0') return 1;
    if (pattern[1] == '*')
        return match_star(pattern[0], pattern + 2, text);
    if (pattern[0] == '$' && pattern[1] == '\0')
        return *text == '\0';
    if (*text != '\0' && match_char(pattern[0], *text))
        return match_here(pattern + 1, text + 1);
    return 0;
}

int match_pattern(const char *pattern, const char *text) {
    if (pattern[0] == '^')
        return match_here(pattern + 1, text);
    do {
        if (match_here(pattern, text)) return 1;
    } while (*text++ != '\0');
    return 0;
}

int glob_match(const char *pattern, const char *text) {
    while (*pattern && *text) {
        if (*pattern == '*') {
            pattern++;
            if (*pattern == '\0') return 1;
            while (*text) {
                if (glob_match(pattern, text)) return 1;
                text++;
            }
            return 0;
        } else if (*pattern == '?' || *pattern == *text) {
            pattern++; text++;
        } else {
            return 0;
        }
    }
    while (*pattern == '*') pattern++;
    return *pattern == '\0' && *text == '\0';
}

int count_words(const char *text) {
    int count = 0, in_word = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == ' ' || text[i] == '\t' || text[i] == '\n') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
    }
    return count;
}

int count_lines(const char *text) {
    int count = 0;
    for (int i = 0; text[i]; i++)
        if (text[i] == '\n') count++;
    return count;
}

void trim_spaces(char *str) {
    int start = 0, end;
    while (str[start] == ' ' || str[start] == '\t') start++;
    end = start;
    while (str[end]) end++;
    end--;
    while (end >= start && (str[end] == ' ' || str[end] == '\t')) end--;
    int len = end - start + 1;
    for (int i = 0; i < len; i++) str[i] = str[start + i];
    str[len] = '\0';
}

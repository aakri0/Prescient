/* t21_state_machine.c — State machines and parsers.
 * Pattern: switch-heavy, many branches, moderate loop counts.
 */

enum TokenType { TOK_NUM, TOK_OP, TOK_PAREN, TOK_SPACE, TOK_END, TOK_ERR };

enum TokenType classify_char(char c) {
    if (c >= '0' && c <= '9') return TOK_NUM;
    if (c == '+' || c == '-' || c == '*' || c == '/') return TOK_OP;
    if (c == '(' || c == ')') return TOK_PAREN;
    if (c == ' ' || c == '\t') return TOK_SPACE;
    if (c == '\0') return TOK_END;
    return TOK_ERR;
}

int tokenize(const char *input, int *types, int max_tokens) {
    int count = 0;
    int state = 0; /* 0=start, 1=in_number */
    for (int i = 0; input[i] && count < max_tokens; i++) {
        enum TokenType t = classify_char(input[i]);
        switch (state) {
        case 0:
            if (t == TOK_NUM)   { state = 1; types[count++] = TOK_NUM; }
            else if (t == TOK_OP)    types[count++] = TOK_OP;
            else if (t == TOK_PAREN) types[count++] = TOK_PAREN;
            else if (t == TOK_ERR)   return -1;
            break;
        case 1:
            if (t != TOK_NUM) {
                state = 0;
                if (t == TOK_OP)    types[count++] = TOK_OP;
                else if (t == TOK_PAREN) types[count++] = TOK_PAREN;
                else if (t == TOK_ERR)   return -1;
            }
            break;
        }
    }
    return count;
}

int parse_integer(const char *s, int *pos) {
    int val = 0, sign = 1;
    if (s[*pos] == '-') { sign = -1; (*pos)++; }
    while (s[*pos] >= '0' && s[*pos] <= '9') {
        val = val * 10 + (s[*pos] - '0');
        (*pos)++;
    }
    return val * sign;
}

/* Simple CSV field counter */
int count_csv_fields(const char *line) {
    int fields = 1, in_quotes = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '"') in_quotes = !in_quotes;
        else if (line[i] == ',' && !in_quotes) fields++;
    }
    return fields;
}

/* HTTP-like status code classifier */
int classify_status(int code) {
    if (code >= 100 && code < 200) return 0; /* informational */
    if (code >= 200 && code < 300) return 1; /* success */
    if (code >= 300 && code < 400) return 2; /* redirect */
    if (code >= 400 && code < 500) return 3; /* client error */
    if (code >= 500 && code < 600) return 4; /* server error */
    return -1;
}

/* Protocol state machine */
int protocol_handler(const int *events, int n) {
    int state = 0, result = 0;
    for (int i = 0; i < n; i++) {
        switch (state) {
        case 0: /* idle */
            if (events[i] == 1) state = 1; /* SYN */
            else if (events[i] == 5) state = 4; /* RST */
            break;
        case 1: /* syn_sent */
            if (events[i] == 2) state = 2; /* SYN-ACK */
            else if (events[i] == 5) state = 0;
            break;
        case 2: /* established */
            if (events[i] == 3) { result++; } /* DATA */
            else if (events[i] == 4) state = 3; /* FIN */
            else if (events[i] == 5) state = 0;
            break;
        case 3: /* closing */
            if (events[i] == 4) state = 0; /* FIN-ACK */
            break;
        case 4: /* reset */
            state = 0;
            break;
        }
    }
    return result;
}

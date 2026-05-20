/* t15_linked_list.c — Linked list operations.
 * Pattern: pointer chasing, moderate branches, low loop depth.
 */

struct Node { int val; struct Node *next; };

int list_length(struct Node *head) {
    int len = 0;
    while (head) { len++; head = head->next; }
    return len;
}

struct Node *list_find(struct Node *head, int val) {
    while (head) {
        if (head->val == val) return head;
        head = head->next;
    }
    return 0;
}

int list_sum(struct Node *head) {
    int sum = 0;
    while (head) { sum += head->val; head = head->next; }
    return sum;
}

int list_max(struct Node *head) {
    if (!head) return 0;
    int mx = head->val;
    head = head->next;
    while (head) {
        if (head->val > mx) mx = head->val;
        head = head->next;
    }
    return mx;
}

struct Node *list_reverse(struct Node *head) {
    struct Node *prev = 0, *curr = head, *next;
    while (curr) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

int list_nth(struct Node *head, int n) {
    for (int i = 0; i < n && head; i++) head = head->next;
    return head ? head->val : -1;
}

int list_is_sorted(struct Node *head) {
    if (!head) return 1;
    while (head->next) {
        if (head->val > head->next->val) return 0;
        head = head->next;
    }
    return 1;
}

int list_count_if(struct Node *head, int threshold) {
    int count = 0;
    while (head) {
        if (head->val > threshold) count++;
        head = head->next;
    }
    return count;
}

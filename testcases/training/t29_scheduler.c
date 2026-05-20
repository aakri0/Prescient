/* t29_scheduler.c — Task scheduling algorithms.
 * Pattern: nested loops, conditional logic, multi-pass algorithms.
 */

#define MAX_TASKS 128

struct Task {
    int id, arrival, burst, priority, deadline;
    int remaining, finish_time, waiting;
};

int fcfs_total_wait(struct Task *tasks, int n) {
    int clock = 0, total_wait = 0;
    for (int i = 0; i < n; i++) {
        if (clock < tasks[i].arrival) clock = tasks[i].arrival;
        total_wait += clock - tasks[i].arrival;
        clock += tasks[i].burst;
        tasks[i].finish_time = clock;
    }
    return total_wait;
}

int sjf_total_wait(struct Task *tasks, int n) {
    /* Sort by burst time (simple selection sort) */
    for (int i = 0; i < n - 1; i++) {
        int min = i;
        for (int j = i + 1; j < n; j++)
            if (tasks[j].burst < tasks[min].burst) min = j;
        if (min != i) {
            struct Task t = tasks[i]; tasks[i] = tasks[min]; tasks[min] = t;
        }
    }
    return fcfs_total_wait(tasks, n);
}

int priority_schedule(struct Task *tasks, int n) {
    /* Sort by priority */
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++)
            if (tasks[j].priority < tasks[best].priority) best = j;
        if (best != i) {
            struct Task t = tasks[i]; tasks[i] = tasks[best]; tasks[best] = t;
        }
    }
    return fcfs_total_wait(tasks, n);
}

int round_robin(struct Task *tasks, int n, int quantum) {
    for (int i = 0; i < n; i++) tasks[i].remaining = tasks[i].burst;
    int clock = 0, done = 0, total_wait = 0;
    while (done < n) {
        int progress = 0;
        for (int i = 0; i < n; i++) {
            if (tasks[i].remaining <= 0) continue;
            int run = tasks[i].remaining < quantum ? tasks[i].remaining : quantum;
            clock += run;
            tasks[i].remaining -= run;
            if (tasks[i].remaining == 0) {
                tasks[i].finish_time = clock;
                total_wait += clock - tasks[i].arrival - tasks[i].burst;
                done++;
            }
            progress = 1;
        }
        if (!progress) break;
    }
    return total_wait;
}

int edf_check_feasible(struct Task *tasks, int n) {
    /* Sort by deadline */
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++)
            if (tasks[j].deadline < tasks[best].deadline) best = j;
        if (best != i) {
            struct Task t = tasks[i]; tasks[i] = tasks[best]; tasks[best] = t;
        }
    }
    int clock = 0;
    for (int i = 0; i < n; i++) {
        if (clock < tasks[i].arrival) clock = tasks[i].arrival;
        clock += tasks[i].burst;
        if (clock > tasks[i].deadline) return 0;
    }
    return 1;
}

double avg_turnaround(struct Task *tasks, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++)
        sum += tasks[i].finish_time - tasks[i].arrival;
    return sum / n;
}

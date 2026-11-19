#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAXP 100
#define MAXSEG 20000
#define NAME_LEN 32

typedef struct {
    int pid, arrival, burst, priority;
    int remaining, first_start, completion;
} Process;

typedef struct {
    int pid;       /* -1 means CPU idle */
    int start, end;
} Segment;

typedef struct {
    Segment s[MAXSEG];
    int n;
} Timeline;

static Process base[MAXP];
static int n;

static void add_segment(Timeline *tl, int pid, int start, int end) {
    if (end <= start) return;
    if (tl->n > 0 && tl->s[tl->n - 1].pid == pid &&
        tl->s[tl->n - 1].end == start) {
        tl->s[tl->n - 1].end = end;
        return;
    }
    if (tl->n >= MAXSEG) {
        fprintf(stderr, "Timeline capacity exceeded.\n");
        exit(EXIT_FAILURE);
    }
    tl->s[tl->n++] = (Segment){pid, start, end};
}

static int earlier_process(const Process p[], int a, int b) {
    if (p[a].arrival != p[b].arrival)
        return p[a].arrival < p[b].arrival;
    return p[a].pid < p[b].pid;
}

static int select_ready(Process p[], int time, int mode) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (p[i].remaining <= 0 || p[i].arrival > time) continue;
        if (best == -1) { best = i; continue; }

        int better = 0;
        if (mode == 0) { /* SJF: shortest original burst */
            if (p[i].burst < p[best].burst) better = 1;
            else if (p[i].burst == p[best].burst &&
                     earlier_process(p, i, best)) better = 1;
        } else if (mode == 1) { /* SRTF */
            if (p[i].remaining < p[best].remaining) better = 1;
            else if (p[i].remaining == p[best].remaining &&
                     earlier_process(p, i, best)) better = 1;
        } else if (mode == 2) { /* FCFS */
            if (earlier_process(p, i, best)) better = 1;
        } else { /* Priority: smaller numeric value is higher */
            if (p[i].priority < p[best].priority) better = 1;
            else if (p[i].priority == p[best].priority &&
                     earlier_process(p, i, best)) better = 1;
        }
        if (better) best = i;
    }
    return best;
}

static void initialize(Process p[]) {
    for (int i = 0; i < n; i++) {
        p[i] = base[i];
        p[i].remaining = p[i].burst;
        p[i].first_start = -1;
        p[i].completion = -1;
    }
}

static int next_arrival(Process p[], int time) {
    int next = INT_MAX;
    for (int i = 0; i < n; i++)
        if (p[i].remaining > 0 && p[i].arrival > time &&
            p[i].arrival < next) next = p[i].arrival;
    return next;
}

static void run_nonpreemptive(Process p[], Timeline *tl, int mode) {
    int done = 0, time = 0;
    while (done < n) {
        int k = select_ready(p, time, mode);
        if (k == -1) {
            int next = next_arrival(p, time);
            if (next == INT_MAX) break;
            add_segment(tl, -1, time, next);
            time = next;
            continue;
        }
        if (p[k].first_start == -1) p[k].first_start = time;
        int end = time + p[k].remaining;
        add_segment(tl, p[k].pid, time, end);
        time = end;
        p[k].remaining = 0;
        p[k].completion = time;
        done++;
    }
}

static void run_srtf(Process p[], Timeline *tl) {
    int done = 0, time = 0;
    while (done < n) {
        int k = select_ready(p, time, 1);
        if (k == -1) {
            int next = next_arrival(p, time);
            if (next == INT_MAX) break;
            add_segment(tl, -1, time, next);
            time = next;
            continue;
        }
        if (p[k].first_start == -1) p[k].first_start = time;
        add_segment(tl, p[k].pid, time, time + 1);
        p[k].remaining--;
        time++;
        if (p[k].remaining == 0) {
            p[k].completion = time;
            done++;
        }
    }
}

static void enqueue_arrivals(Process p[], int time, int queued[],
                             int queue[], int *tail) {
    for (int i = 0; i < n; i++) {
        if (p[i].arrival == time && !queued[i] && p[i].remaining > 0) {
            queue[(*tail)++] = i;
            queued[i] = 1;
        }
    }
}

static void run_round_robin(Process p[], Timeline *tl, int quantum) {
    int queue[MAXP * 4], queued[MAXP] = {0};
    int head = 0, tail = 0, done = 0, time = 0;

    while (done < n) {
        enqueue_arrivals(p, time, queued, queue, &tail);

        if (head == tail) {
            int next = next_arrival(p, time);
            if (next == INT_MAX) break;
            add_segment(tl, -1, time, next);
            time = next;
            continue;
        }

        int k = queue[head++];
        int slice = 0;
        if (p[k].first_start == -1) p[k].first_start = time;

        while (slice < quantum && p[k].remaining > 0) {
            add_segment(tl, p[k].pid, time, time + 1);
            p[k].remaining--;
            time++;
            slice++;
            enqueue_arrivals(p, time, queued, queue, &tail);
        }

        if (p[k].remaining == 0) {
            p[k].completion = time;
            done++;
        } else {
            queue[tail++] = k;
        }

        /* Compact queue before its fixed storage can be exhausted. */
        if (head > MAXP && head * 2 > tail) {
            int count = tail - head;
            memmove(queue, queue + head, (size_t)count * sizeof(int));
            tail = count;
            head = 0;
        }
        if (tail >= MAXP * 4 - MAXP) {
            int count = tail - head;
            memmove(queue, queue + head, (size_t)count * sizeof(int));
            tail = count;
            head = 0;
        }
    }
}

static void print_results(const char *name, Process p[], Timeline *tl) {
    double avg_wait = 0, avg_turn = 0, avg_resp = 0;
    printf("\n========== %s ==========\n", name);
    printf("\nGantt chart:\n");
    for (int i = 0; i < tl->n; i++) {
        if (tl->s[i].pid == -1) printf("| IDLE ");
        else printf("| P%d ", tl->s[i].pid);
    }
    printf("|\n");
    if (tl->n > 0) {
        printf("%d", tl->s[0].start);
        for (int i = 0; i < tl->n; i++) printf("\t%d", tl->s[i].end);
        printf("\n");
    }

    printf("\nPID\tAT\tBT\tPR\tCT\tTAT\tWT\tRT\n");
    for (int i = 0; i < n; i++) {
        int tat = p[i].completion - p[i].arrival;
        int wt = tat - p[i].burst;
        int rt = p[i].first_start - p[i].arrival;
        printf("P%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
               p[i].pid, p[i].arrival, p[i].burst, p[i].priority,
               p[i].completion, tat, wt, rt);
        avg_turn += tat;
        avg_wait += wt;
        avg_resp += rt;
    }
    printf("\nAverage waiting time    : %.2f\n", avg_wait / n);
    printf("Average turnaround time : %.2f\n", avg_turn / n);
    printf("Average response time   : %.2f\n", avg_resp / n);
}

static void execute_algorithm(int choice, int quantum) {
    Process p[MAXP];
    Timeline tl = {{{0}}, 0};
    initialize(p);

    switch (choice) {
        case 1: run_nonpreemptive(p, &tl, 2); print_results("FCFS", p, &tl); break;
        case 2: run_nonpreemptive(p, &tl, 0); print_results("SJF (Non-preemptive)", p, &tl); break;
        case 3: run_srtf(p, &tl); print_results("SRTF", p, &tl); break;
        case 4: run_round_robin(p, &tl, quantum); print_results("Round Robin", p, &tl); break;
        case 5: run_nonpreemptive(p, &tl, 3); print_results("Priority (Non-preemptive)", p, &tl); break;
        default: break;
    }
}

int main(void) {
    int choice, quantum = 2;
    printf("CPU Scheduling Simulator - Topic 4\n");
    printf("Enter number of processes (1-%d): ", MAXP);
    if (scanf("%d", &n) != 1 || n < 1 || n > MAXP) {
        fprintf(stderr, "Invalid process count.\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < n; i++) {
        base[i].pid = i + 1;
        printf("\nProcess P%d\n", i + 1);
        printf("Arrival time: ");
        if (scanf("%d", &base[i].arrival) != 1 || base[i].arrival < 0) {
            fprintf(stderr, "Arrival time must be a non-negative integer.\n");
            return EXIT_FAILURE;
        }
        printf("CPU burst: ");
        if (scanf("%d", &base[i].burst) != 1 || base[i].burst <= 0) {
            fprintf(stderr, "CPU burst must be a positive integer.\n");
            return EXIT_FAILURE;
        }
        printf("Priority (smaller number = higher priority): ");
        if (scanf("%d", &base[i].priority) != 1) {
            fprintf(stderr, "Invalid priority.\n");
            return EXIT_FAILURE;
        }
    }

    for (;;) {
        printf("\nChoose algorithm:\n");
        printf("1. FCFS\n2. SJF (non-preemptive)\n3. SRTF\n");
        printf("4. Round Robin\n5. Priority (non-preemptive)\n");
        printf("6. Run all algorithms\n0. Exit\nChoice: ");
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "Invalid menu input.\n");
            return EXIT_FAILURE;
        }
        if (choice == 0) break;
        if (choice < 0 || choice > 6) {
            printf("Please choose a valid menu option.\n");
            continue;
        }
        if (choice == 4 || choice == 6) {
            printf("Time quantum (positive integer): ");
            if (scanf("%d", &quantum) != 1 || quantum <= 0) {
                fprintf(stderr, "Time quantum must be positive.\n");
                return EXIT_FAILURE;
            }
        }
        if (choice == 6) {
            for (int a = 1; a <= 5; a++) execute_algorithm(a, quantum);
        } else {
            execute_algorithm(choice, quantum);
        }
    }
    printf("Simulation finished.\n");
    return EXIT_SUCCESS;
}

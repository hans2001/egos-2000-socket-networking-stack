#include "process.h"
#include <string.h>
#include "queue.h"

static queue_t q_high, q_mid, q_low;

/* [lab3-ex3]
 * initialize MLFQ data structures */
static void mlfq_init() {
    queue_init(&q_high);
    queue_init(&q_mid);
    queue_init(&q_low);
}

static void remove_pid(int pid) {
    try_rm_item(&q_high, &proc_set[pid2idx(pid)]); 
    try_rm_item(&q_mid, &proc_set[pid2idx(pid)]); 
    try_rm_item(&q_low, &proc_set[pid2idx(pid)]); 
}

static int dequeue_runnable(queue_t *q) {
    int count = 0;

    for (node_t *n = q->head; n != (void *)0; n = n->next)
        count++;

    while (count-- > 0) {
        struct process *p = (struct process *) dequeue(q);        

        if (p == (void *)0) 
            break;

        if (p->status == PROC_READY || p->status == PROC_RUNNABLE) {
            return p->pid;
        }
        enqueue(q, p);
    } 
    return 0;
}
 
static int pick_next_pid() {   
    int pid = dequeue_runnable(&q_high);   

    if (pid == 0) pid = dequeue_runnable(&q_mid);  
    if (pid == 0) pid = dequeue_runnable(&q_low);  

    return pid;   
} 

static int level_for_pid(int pid) { 
    if (pid < USER_PID_START) return 0;

    struct process *p = &proc_set[pid2idx(pid)];
    unsigned long long t = p->schd_attr.longlongs[3];

    if (t <= QUANTUM) return 0;
    if (t < 2 * QUANTUM) return 1;

    return 2;
} 

static void enqueue_pid(int pid) {
    remove_pid(pid);

    struct process *p = &proc_set[pid2idx(pid)];

    if (p->status == PROC_UNUSED) return;

    int lvl = level_for_pid(pid);

    if (lvl == 0) enqueue(&q_high, p);
    else if (lvl == 1) enqueue(&q_mid, p);
    else enqueue(&q_low, p);

    return;
}

/* [lab3-ex3]
 * Implement the MLFQ scheduler.
 *
 * Requirements:
 *   - Return the pid of the next runnable process.
 *   - If no other process is runnable and the current process is in
 *     PROC_RUNNING, allow the current process to continue running.
 *   - If no process is runnable, return 0.
 *   - Always place system processes (pid < USER_PID_START) in the
 *     highest-priority queue.
 *   - Do not enqueue processes of state PROC_UNUSED.
 *
 * Hints:
 *   - Check the process state before selecting it to run; a dequeued
 *     process may no longer be runnable (e.g., it may be waiting).remove_pid
 *   - Maintain the following invariants for a robust MLFQ implementation:
 *       -- The currently running process is not present in any queue.
 *       -- All processes except the currently running one appear in exactly one queue.
 *       -- No pid appears in more than one queue.
 *   - Use pid2idx() and idx2pid() to translate between PID and index in
 *     `proc_set` for code robustness.
 */
int mlfq() {
    if (curr_status == PROC_RUNNING ||
        curr_status == PROC_RUNNABLE ||
        curr_status == PROC_READY) {
        remove_pid(curr_pid);
    }
    int next_pid  = pick_next_pid();

    if (next_pid == 0){
        if (curr_status == PROC_RUNNING || curr_status == PROC_RUNNABLE) return curr_pid;
        return 0;
    }

    if (curr_status == PROC_READY || curr_status == PROC_RUNNABLE) {
        enqueue_pid(curr_pid);
    }

    return next_pid;
}


/* [lab3-ex3]
 * TODO: Update the MLFQ-related information for pid. */
void mlfq_update(int pid, enum state_transition tran, int time_units) {
    // initalize MLFQ; first called by proc_sys before schedule()
    static int initialized = 0;
    if (!initialized) {
        mlfq_init();
        initialized++;
    }

    switch (tran) {
        case PROC_ON_ARRIVE:
            enqueue_pid(pid);
            break;
        case PROC_ON_SCHED_IN:
            remove_pid(pid);
            break;
        case PROC_ON_SCHED_OUT:
            enqueue_pid(pid);
            break;
        case PROC_ON_STOP:
            remove_pid(pid);
            break;
        default:
            break;
    }
}

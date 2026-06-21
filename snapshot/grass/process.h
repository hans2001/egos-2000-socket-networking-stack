#pragma once

#include "egos.h"
#include "syscall.h"
#include "servers.h"

enum proc_status {
    PROC_UNUSED,
    PROC_LOADING,  /* allocated and wait for loading elf binary */
    PROC_READY,    /* finished loading elf and wait for first running */
    PROC_RUNNING,
    PROC_RUNNABLE,
    PROC_PENDING_SYSCALL,
    PROC_SLEEPING
};

struct process {
    int pid;
    struct syscall syscall;
    enum proc_status status;
    uint mepc, saved_registers[32];
    // scheduling attributes
    union {
        unsigned char      chars[64];
        unsigned int       ints[16];
        float              floats[16];
        unsigned long long longlongs[8];
        double             doubles[8];
    } schd_attr;
};

#define MAX_NPROCESS 16
#define USER_PID_START GPID_USER_START

/* process management */
int proc_alloc();
void proc_free(int);
void proc_sleep(int pid, uint usec);

/* process status management */
void proc_set_ready(int);
void proc_set_running(int);
void proc_set_runnable(int);
void proc_set_pending(int);

/* Process state transition events and their associated callbacks */
enum state_transition {
    PROC_ON_ARRIVE=1,
    PROC_ON_SCHED_IN,
    PROC_ON_SCHED_OUT,
    PROC_ON_STOP,
    PROC_ON_SLEEP
};

void proc_on_arrive(int pid);
void proc_on_sched_in(int pid);
void proc_on_sched_out(int pid);
void proc_on_stop(int pid);
void proc_on_sleep(int pid, int time_units);

/* multi-core support */
void proc_coresinfo();
extern uint core_to_proc_idx[NCORES];

/* helper functions */
int pid2idx(int pid);
int idx2pid(int proc_idx);

/* manaing process status */
/* defined in kernel.c */
extern uint core_in_kernel;
extern uint core_to_proc_idx[NCORES];
extern struct process proc_set[MAX_NPROCESS + 1];

#define curr_proc_idx (core_to_proc_idx[core_in_kernel])
#define curr_pid      (proc_set[curr_proc_idx].pid)
#define curr_status   (proc_set[curr_proc_idx].status)
#define curr_saved    (proc_set[curr_proc_idx].saved_registers)


/* kernel functions */
void proc_yield();  /* defined in scheduler.c */
void proc_try_syscall(struct process* proc); /* defined in syscall.c */

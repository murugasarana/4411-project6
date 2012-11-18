#ifndef __SCHEDULER_H_
#define __SCHEDULER_H_

#include "minithread.h"

typedef struct scheduler *scheduler_t;

int scheduler_initialize();

void scheduler_set_idle(minithread_t idle);

/*
 * Return 0 on success, -1 on failure.
 */
int scheduler_schedule(minithread_t thread);

/*
 * Return the next thread in the scheduling order.
 */
minithread_t scheduler_next_thread();

/*
 * Return the number of threads in the scheduler.
 */
int scheduler_size();

/*
 * Return 1 if it is time to run a new thread,
 * 0 if this thread is still within its quota.
 */
int scheduler_quota_expired();

/*
 * Let the scheduler know that a tick has passed.
 */
void scheduler_advance();

#endif __SCHEDULER_H_
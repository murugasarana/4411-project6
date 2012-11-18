    #include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "scheduler.h"
#include "minithread.h"
#include "multilevel_queue.h"
#include "interrupts.h"

#define SCHEDULER_LEVELS 4
#define TICKS_LEVEL_0 80
#define TICKS_LEVEL_1 40
#define TICKS_LEVEL_2 24
#define TICKS_LEVEL_3 16

multilevel_queue_t queue;
int size;
int current_level;
int thread_ticks_remaining; // before a scheduling change
int level_ticks_remaining; // before switching levels
minithread_t idle;

int
scheduler_initialize() {
	queue = multilevel_queue_new(SCHEDULER_LEVELS);
	if (queue == NULL) {
		fprintf(stderr, "Out of memory in scheduler_new");
		return -1;
	}
	size = 0;
	current_level = 0;
	thread_ticks_remaining = 1;
	level_ticks_remaining = TICKS_LEVEL_0;
	return 0;
}

void
scheduler_set_idle(minithread_t idle_thread) {
	idle = idle_thread;
}

/* Ignores the idle thread.
 */
int
scheduler_schedule(minithread_t thread) {
	int level, success;

	if (minithread_get_status(thread) == OK) {
		level = minithread_priority(thread);
		success = multilevel_queue_enqueue(queue, level, thread);
		if (success == -1) {
			return -1;
		}
		size += 1;
	}
	return 0;
}

minithread_t
scheduler_next_thread() {
	void* out = NULL;
    if (size == 0) {
		out = idle;
	} else {
        multilevel_queue_dequeue(queue, current_level, &out);
        if(out == NULL) {
            return NULL;
        }
        size -= 1;
	}
	return (minithread_t)out;
}

int
scheduler_size() {
	return size;
}

int
scheduler_quota_expired() {
	return thread_ticks_remaining == 0 ? 1 : 0;
}

void
scheduler_advance() {
	level_ticks_remaining--;
	if (level_ticks_remaining < 0) {
		current_level = (current_level + 1) % SCHEDULER_LEVELS;
		switch (current_level) {
		case 0: 
			level_ticks_remaining = TICKS_LEVEL_0;
			break;
		case 1: 
			level_ticks_remaining = TICKS_LEVEL_1;
			break;
		case 2: 
			level_ticks_remaining = TICKS_LEVEL_2;
			break;
		case 3: 
			level_ticks_remaining = TICKS_LEVEL_3;
			break;
		}
	}
	thread_ticks_remaining--;
	if (thread_ticks_remaining < 0) {
		thread_ticks_remaining = 1 << current_level;
	}
}

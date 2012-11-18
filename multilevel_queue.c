/*
 * Multilevel queue manipulation functions  
 */
#include "multilevel_queue.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

struct multilevel_queue {
	int number_of_levels;
	int size;
	queue_t *levels; // array of pointers to queues
};

/*
 * Returns an empty multilevel queue with number_of_levels levels. On error should return NULL.
 */
multilevel_queue_t
multilevel_queue_new(int number_of_levels) {
	int i;
	multilevel_queue_t queue = (multilevel_queue_t)malloc(sizeof(struct multilevel_queue));
	if (queue == NULL) {
		return NULL;
	}
	queue->number_of_levels = number_of_levels;
	queue->size = 0;
	queue->levels = (queue_t *)malloc(number_of_levels * sizeof(queue_t));
	if (queue->levels == NULL) {
		return NULL;
	}
	for (i = 0; i < number_of_levels; i++) {
		queue->levels[i] = queue_new();
		if (queue->levels[i] == NULL) {
			return NULL;
		}
	}
	return queue;
}

/*
 * Appends an void* to the multilevel queue at the specified level. Return 0 (success) or -1 (failure).
 */
int
multilevel_queue_enqueue(multilevel_queue_t queue, int level, void* item) {
	int success;
	if (queue == NULL || level < 0 || level > queue->number_of_levels) {
		return -1;
	}
	success = queue_append(queue->levels[level], item);
	if (success == -1) {
		return -1;
	}
	queue->size += 1;
	return 0;
}

/*
 * Dequeue and return the first void* from the multilevel queue starting at the specified level. 
 * Levels wrap around so as long as there is something in the multilevel queue an item should be returned.
 * Return the level that the item was located on and that item if the multilevel queue is nonempty,
 * or -1 (failure) and NULL if queue is empty.
 */
int
multilevel_queue_dequeue(multilevel_queue_t queue, int level, void** item) {
	if (queue == NULL || queue->size == 0) {
		*item = NULL;
		return -1;
	}
	for (*item = NULL; *item == NULL; ) {
		queue_dequeue(queue->levels[level], item);
		if (*item == NULL) {
			level = (level + 1) % queue->number_of_levels;
		}
	}
	queue->size -= 1;
	return level;
}

/* 
 * Free the queue and return 0 (success) or -1 (failure). Do not free the queue nodes; this is
 * the responsibility of the programmer.
 */
int
multilevel_queue_free(multilevel_queue_t queue) {
	int i;
	if (queue == NULL || queue->levels == NULL) {
		return -1;
	}
	for (i = 0; i < queue->number_of_levels; i++) {
		if (queue_free(queue->levels[i]) == -1) {
			return -1;
		}
	}
	free(queue->levels);
	free(queue);
	return 0;
}


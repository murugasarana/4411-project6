#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "multilevel_queue.h"

void
test_enqueue_dequeue() {
	multilevel_queue_t queue = multilevel_queue_new(4);
	int w = 0;
	int x = 1;
	int y = 2;
	int z = 3;
	void *out;
	multilevel_queue_enqueue(queue, 0, &w);
	multilevel_queue_enqueue(queue, 1, &x);
	multilevel_queue_enqueue(queue, 2, &y);
	multilevel_queue_enqueue(queue, 3, &z);
	multilevel_queue_dequeue(queue, 0, &out);
	assert(*(int*)out == 0);
	multilevel_queue_dequeue(queue, 1, &out);
	assert(*(int*)out == 1);
	multilevel_queue_dequeue(queue, 2, &out);
	assert(*(int*)out == 2);
	multilevel_queue_dequeue(queue, 3, &out);
	assert(*(int*)out == 3);
	multilevel_queue_free(queue);
}

void
test_dequeue_empty() {
	
}

void
test_dequeue_level_empty() {
	multilevel_queue_t queue;
	int x, level;
	void *out;

	x = 1;
	queue = multilevel_queue_new(4);
	multilevel_queue_enqueue(queue, 3, &x);
	level = multilevel_queue_dequeue(queue, 0, &out);
	assert(*(int*)out == 1);
	assert(level == 3);
	multilevel_queue_free(queue);
}

void
test_dequeue_wrap() {
	multilevel_queue_t queue;
	int x, level;
	void *out;
	
	x = 1;
	queue = multilevel_queue_new(4);
	multilevel_queue_enqueue(queue, 2, &x);
	level = multilevel_queue_dequeue(queue, 3, &out);
	assert(*(int*)out == 1);
	assert(level == 2);
	multilevel_queue_free(queue);
}

void
main() {
	fprintf(stdout, "Testing multilevel_queue\n");
	test_enqueue_dequeue();
	test_dequeue_empty();
	test_dequeue_level_empty();
	test_dequeue_wrap();
	fprintf(stdout, "Done!\n");
}
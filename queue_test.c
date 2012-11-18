#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

void
test_append(void) {
	int x = 1;
	int y = 2;
	void *out;
	queue_t queue = queue_new();
	queue_dequeue(queue, &out);
	assert(out == NULL);
	queue_append(queue, &x);
	queue_append(queue, &y);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 1);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 2);
	queue_dequeue(queue, &out);
	assert(out == NULL);
	queue_free(queue);
}

void
test_prepend(void) {
	int x = 1;
	int y = 2;
	void *out;
	queue_t queue = queue_new();
	queue_dequeue(queue, &out);
	assert(out == NULL);
	queue_prepend(queue, &x);
	queue_prepend(queue, &y);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 2);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 1);
	queue_dequeue(queue, &out);
	assert(out == NULL);
	queue_free(queue);
}

void
test_length(void) {
	assert(1);
}

void
test_delete(void) {
	assert(1);
}

void
test_iterate(void) {
	assert(1);
}

int compare_ints(void *a, void *b) {
	if (*(int*)a == *(int*)b) {
		return 0;
	}
	if (*(int*)a < *(int*)b) {
		return -1;
	}
	return 1;
}

void
test_insert_sorted(void) {
	int x = 1;
	int y = 2;
	int z = 3;
	int q = 4;
	int r = 5;
	void *out;
	queue_t queue = queue_new();
	queue_insert_sorted(queue, &compare_ints, &r);
	queue_insert_sorted(queue, &compare_ints, &x);
	queue_insert_sorted(queue, &compare_ints, &z);
	queue_insert_sorted(queue, &compare_ints, &y);
	queue_insert_sorted(queue, &compare_ints, &q);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 1);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 2);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 3);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 4);
	queue_dequeue(queue, &out);
	assert(*(int*)out == 5);
	queue_free(queue);
}

void
main(void) {
	fprintf(stdout, "Testing queue...\n");
	test_append();
	test_prepend();
	test_length();
	test_delete();
	test_iterate();
	test_insert_sorted();
	fprintf(stdout, "Done!\n");
}

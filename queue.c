/*
 * Generic queue implementation.
 *
 */
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
/*
 * q_node_t is a pointer to
 */
typedef struct queue_node {
	void* data;
	struct queue_node* prev;
	struct queue_node* next;
} *q_node_t;

/*
 * queue_t is a pointer to
 */
struct queue {
	q_node_t head;
	q_node_t tail;
    int q_length;
};

/*
 * Return an empty queue.
 */
queue_t
queue_new() {
	queue_t queue = (queue_t)malloc(sizeof(struct queue));
	if(NULL == queue) {
		fprintf(stderr, "NO MEMORY!!!\n");
	} else {
		queue -> head = NULL;
		queue -> tail = NULL;
        queue -> q_length = 0;
    }
	return queue;
}

/*
 * Prepend a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
int
queue_prepend(queue_t queue, void* item) {
	q_node_t node;
    if(NULL == queue) {
		return -1;
	}
	assert((queue->head != NULL && queue->tail != NULL) || (queue-> head == NULL && queue-> tail == NULL));
    node = (q_node_t)malloc(sizeof(struct queue_node));
	if (node == NULL) {
		return -1;
	}
	node -> prev = NULL;
	node -> data = item;
	queue -> head = node;
	queue -> q_length += 1;
	// When the queue is empty
	if(NULL == queue -> head && NULL == queue -> tail) {
		node -> next = NULL;
		queue -> tail = node;
	} else {
		node -> next = queue -> head;
		queue -> head -> prev = node;
	}
	assert(queue->head != NULL);
	assert(queue->tail != NULL);
    return 0;
}

/*
 * Append a void* to a queue (both specifed as parameters). Return
 * 0 (success) or -1 (failure).
 */
int
queue_append(queue_t queue, void* item) {
	q_node_t node;
	if( NULL == queue) {
		return -1;
	}
	node = (q_node_t)malloc(sizeof(struct queue_node));
	if (NULL == node) {
		return -1;
	}
	if( NULL == queue -> head && NULL == queue -> tail) { // Empty Queue
		node -> data = item;
		node -> prev = NULL;
		node -> next = NULL;
		queue -> head = node;
		queue -> tail = node;
        queue -> q_length += 1;
	} else { // Non Empty Queue
		node -> data = item;
		node -> prev = queue -> tail;
		node -> next = NULL;
		queue -> tail -> next = node;
		queue -> tail = node;
        queue -> q_length += 1;
	}
	assert(queue->head != NULL);
	assert(queue->tail != NULL);
	return 0;
}

/*
 * Dequeue and return the first void* from the queue or NULL if queue
 * is empty.  Return 0 (success) or -1 (failure).
 */
int
queue_dequeue(queue_t queue, void** item) {
	q_node_t temp;
	if(queue == NULL) {
		return -1;
	}
    assert(!(queue->head == NULL && queue->tail != NULL ) );
	if(queue->head == NULL && queue->tail == NULL) {
		*item = NULL;
		return -1;
	}
	temp = queue->head;
	*item = queue->head->data;
	//Indicates that there is only one element in the queue
	if(queue->head->next == NULL && queue->tail->prev==NULL ){
		queue->head = NULL;
		queue->tail = NULL;
	} else {
		queue->head = queue->head->next;
		queue->head->prev = NULL;
	}
	queue->q_length -= 1;
	free(temp);
	assert((queue->head != NULL && queue->tail != NULL) ||
		(queue->head == NULL && queue->tail == NULL));
	
    return 0;
}

/*
 * Iterate the function parameter over each element in the queue.  The
 * additional void* argument is passed to the function as its first
 * argument and the queue element is the second.  Return 0 (success)
 * or -1 (failure).
 */
int
queue_iterate(queue_t queue, PFany f, void* item) {
	q_node_t current;
	int func_ret_val;
    if(NULL == queue || NULL == f){
        return -1;
    }
	current = queue->head;
	while(current != NULL) {
		func_ret_val = (*f)(item, current->data);
		if(func_ret_val == -1) {
			return -1;
		}
		current = current->next;
	}
	assert((queue->head != NULL && queue->tail != NULL) ||
		(queue->head == NULL && queue->tail == NULL));
	return 0;
}

/*
 * Free the queue and return 0 (success) or -1 (failure).
 */
int
queue_free(queue_t queue) {
	q_node_t current;
	q_node_t temp;
	if (queue == NULL) {
		return -1;
	}
	current = queue->head;
	while(current != NULL) {
		temp = current;
		current = current->next;
		free(temp);
	}
	free(queue);
	return 0;
}

/*
 * Return the number of items in the queue.
 */
int
queue_length(queue_t queue) {
	if(queue == NULL) {
		return -1;
	}
    return queue->q_length;
}

/*
 * Delete the specified item from the given queue.
 * Return -1 on error.
 */
int
queue_delete(queue_t queue, void** item) {
	q_node_t q_node_ptr = queue -> head;
	if( NULL == queue) {
		return 0;
	} else if(queue -> head -> data == *item ) { //delete at head
        //head is not the only node
        if(NULL != q_node_ptr -> next) {
            q_node_ptr -> next -> prev = NULL;
            queue -> head = q_node_ptr -> next;
        } else {
            //only node in queue is deleted, return empty queue.
        }
		free(q_node_ptr);
		return 0;
	} else if(queue -> tail -> data == *item ) { //delete at tail
		q_node_ptr -> prev -> next = NULL;
		queue -> tail = q_node_ptr -> prev;
		free(q_node_ptr);
		return 0;
	} else {
		while( q_node_ptr -> data != *item) {
			if(q_node_ptr -> next == NULL) {
				return -1; //item not found in queue, so send fail
			}
			q_node_ptr = q_node_ptr -> next;
		}
	}
	q_node_ptr -> prev -> next = q_node_ptr -> next;
	q_node_ptr -> next -> prev = q_node_ptr -> prev;
	free(q_node_ptr);
	return 0;
}

/*
 * Insert the specified item in sorted order,
 * precondition that the queue is already sorted.
 * Parameter compare is a function that compares two
 * items in the queue and returns -1, 0, 1 if the new item
 * is less than, equal to, or greater than the queued item.
 */
int
queue_insert_sorted(queue_t queue, PFany compare, void *item) {
	q_node_t current;
	q_node_t new_node;
	if (queue == NULL || compare == NULL) {
		return -1;
	}
	new_node = (q_node_t)malloc(sizeof(struct queue_node));
	if (new_node == NULL) {
		fprintf(stderr, "No memory!!!");
		return -1;
	}
	new_node->data = item;
	new_node->next = NULL;
	new_node->prev = NULL;
	// empty queue
	if (queue->head == NULL && queue->tail == NULL) {
		queue->head = new_node;
		queue->tail = new_node;
	// replace head
	} else if ((*compare)(queue->head->data, item) >= 0) {
		new_node->next = queue->head;
		queue->head->prev = new_node;
		queue->head = new_node;
	// replace tail
	} else if ((*compare)(queue->tail->data, item) < 0) {
		new_node->prev = queue->tail;
		queue->tail->next = new_node;
		queue->tail = new_node;
	// replace inner
	} else {
		current = queue->head;
		while ((*compare)(current->data, item) < 0) {
			current = current->next;
		}
		new_node->prev = current->prev;
		current->prev->next = new_node;
		new_node->next = current;
		current->prev = new_node;
	}
	queue->q_length += 1;
	assert(queue->head != NULL);
	assert(queue->tail != NULL);
	return 0;
}

void*
queue_delete_by_predicate(queue_t queue, PFany predicate, void* arg2) {
	q_node_t current;
	q_node_t temp;
    void* queue_item = NULL;
	int success;
	// allow deleting null
	if (queue == NULL || predicate == NULL) {
		return NULL;
	}
	for (current = queue->head; current != NULL; ) {
		success = predicate(current->data, arg2);
		if (success == 0) {
			temp = current;
			if (current->next != NULL) {
				current->next->prev = current->prev;
			}
			if (current->prev != NULL) {
				current->prev->next = current->next;
			}
			if (queue->head == current) {
				if (current->prev != NULL) {
					queue->head = current->prev;
				} else {
					// edge case with deleting the first of 2 elements
					queue->head = current->next;
				}
			}
			if (queue->tail == current) {
				if (current->next != NULL) {
					queue->tail = current->next;
				} else {
					// edge case with deleting the last of 2 elements
					queue->tail = current->prev;
				}
			}
			current = current->next;
            queue_item = temp->data;
			free(temp);
			queue->q_length--;
		} else {
			current = current->next;
		}
	}
    return queue_item;
}

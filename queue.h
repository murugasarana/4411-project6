/*
 * Generic queue manipulation functions  
 */
#ifndef __QUEUE_H__
#define __QUEUE_H__

/*
 * PFany is a pointer to a function that can take two void* arguments
 * and return an integer.
 */
typedef int (*PFany)(void*, void*);

/*
 * queue_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how queues are
 * represented.  They see and manipulate only queue_t's. 
 */
typedef struct queue* queue_t;

/*
 * Return an empty queue. On error should return NULL.
 */
extern queue_t queue_new();

/*
 * Prepend a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
extern int queue_prepend(queue_t, void*);

/*
 * Appends a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
extern int queue_append(queue_t, void*);

/*
 * Dequeue and return the first void* from the queue. Return 0
 * (success) and first item if queue is nonempty, or -1 (failure) and
 * NULL if queue is empty.
 */
extern int queue_dequeue(queue_t, void**);

/*
 * Iterate the function parameter over each element in the queue.  The
 * additional void* argument is passed to the function as its first
 * argument and the queue element is the second.  
 * Return 0 (success) or -1 (failure).
 */
extern int queue_iterate(queue_t, PFany, void*);

/* 
 * Free the queue and return 0 (success) or -1 (failure).
 */
extern int queue_free (queue_t);

/*
 * Return the number of items in the queue.
 */
extern int queue_length(queue_t queue);

/* 
 * Delete the specified item from the given queue. 
 * Return -1 on error.
 */
extern int queue_delete(queue_t queue, void** item);

/*
 * Insert the specified item in sorted order,
 * precondition that the queue is already sorted.
 * Parameter compare is a function that compares two
 * items in the queue and returns -1, 0, 1 if the new item
 * is less than, equal to, or greater than the queued item.
 */
extern int queue_insert_sorted(queue_t queue, PFany compare, void *item);

/*
 * Delete any nodes in the queue whose data satisfy the predicate.
 * predicate returns 0 if an item should be deleted, -1 if not.
 * arg2 is the second argument to the predicate function, the queue item
 * is the first.
 */
extern void* queue_delete_by_predicate(queue_t queue, PFany predicate, void* arg2);

#endif __QUEUE_H__

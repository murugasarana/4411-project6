#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "synch.h"
#include "queue.h"
#include "minithread.h"
#include "interrupts.h"

/*
 * Semaphores.
 */
struct semaphore {
    int cnt;
    queue_t sema_queue;
    tas_lock_t l;
};


/*
 * semaphore_t semaphore_create()
 *	Allocate a new semaphore.
 */
semaphore_t semaphore_create() {
    semaphore_t sema = (semaphore_t)malloc(sizeof(struct semaphore));
    if(NULL == sema) {
        fprintf(stderr, "NO MEMORY");
    }
    sema -> sema_queue = queue_new();
    if(NULL == sema -> sema_queue){
        fprintf(stderr, "NO MEMORY");
    }
    sema -> cnt = 0;
    atomic_clear(&(sema -> l));
    return sema;
}

/*
 * semaphore_destroy(semaphore_t sem);
 *	Deallocate a semaphore.
 */
void
semaphore_destroy(semaphore_t sem) {
    queue_free(sem -> sema_queue);
    free(sem);
}

 
/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *	initialize the semaphore data structure pointed at by
 *	sem with an initial value cnt.
 */
void
semaphore_initialize(semaphore_t sem, int cnt) {
    sem -> cnt = cnt;
}


/*
 * semaphore_P(semaphore_t sem)
 *	P on the sempahore. Your new implementation should use TAS locks.
 */
void
semaphore_P(semaphore_t sem) {
    interrupt_level_t level;
    
    while(1 == atomic_test_and_set(&(sem -> l)));
    level = set_interrupt_level(DISABLED);
    sem -> cnt -=1;
    
    // since the internal count is <0 put the thread to the sema_queue and put it to sleep.
    if(sem -> cnt <0) {
        queue_append(sem -> sema_queue, minithread_self());
        set_interrupt_level(level);
        minithread_unlock_and_stop(&(sem -> l));
    } else {
        atomic_clear(&(sem -> l));
    }
    set_interrupt_level(level);
}

/*
 * semaphore_V(semaphore_t sem)
 *	V on the sempahore. Your new implementation should use TAS locks.
 */
void
semaphore_V(semaphore_t sem) {
    void* item;
    minithread_t wake_thread;
    interrupt_level_t level;
    while(1 == atomic_test_and_set(&(sem -> l)));
    level = set_interrupt_level(DISABLED);
    sem -> cnt += 1;
    
    // restart the wake_thread by dequeueing it from the sema_queue
    if( sem -> cnt <= 0) {
        if(queue_dequeue( sem -> sema_queue, &item )== 1) {
            fprintf(stdout, "dequeue error in semaphore_V\n");
        }
        wake_thread = (minithread_t) item;
        minithread_start(wake_thread);
    }
    atomic_clear(&(sem -> l));
    set_interrupt_level(level);
}

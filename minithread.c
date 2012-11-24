    /*
 * minithread.c:
 *	This file provides a few function headers for the procedures that
 *	you are required to implement for the minithread assignment.
 *
 *	EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *	NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "minithread.h"
#include "disk.h"
#include "minifile.h"
#include "queue.h"
#include "interrupts.h"
#include "synch.h"
#include "scheduler.h"
#include "alarm.h"
#include "network.h"
#include "miniroute.h"
#include "minimsg.h"
#include "miniheader.h"
#include "minisocket.h"

#include <assert.h>

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */

struct minithread {
	int thread_id;
	int priority;
	enum status status;
    stack_pointer_t stackbase;
    stack_pointer_t stacktop;
};

minithread_t reaper_thread;// the reaper thread who cleans dead threads
semaphore_t schedule_reaper_thread;

// the queue where dead threads go into once they finish executing finalproc
queue_t delete_queue;

minithread_t active_thread; // the currently running thread

queue_t alarm_queue;
minithread_t alarm_thread;
semaphore_t alarm_sema;

int finalproc(arg_t);
int reap_proc(arg_t);
int update_alarm_item_delay(void* , void* );
int trigger_alarm(void* ,void* );
int alarm_proc(int* arg);
void minithread_wake(semaphore_t);
int minithread_get_status(minithread_t);


int minithread_get_status(minithread_t thread) {
    return thread->status;
}

/* Interrupt handlers */

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void
clock_handler(void* arg) {
    interrupt_level_t l;
    l = set_interrupt_level(DISABLED);
    ticks += 1;
	scheduler_advance();
    queue_iterate(alarm_queue, &trigger_alarm, NULL);
    
	if (scheduler_quota_expired()) {
        minithread_yield();
	}
    set_interrupt_level(l);
}

void
network_handler(void *arg) {
	int  receiver_port;
	miniport_t port;
    network_interrupt_arg_t *interrupt;
    routing_header_t routeheader;
    char routing_type;
    mini_header_t dataheader;
    char protocol;
    interrupt_level_t level;

	printf("Got packet\n");
    interrupt = (network_interrupt_arg_t *) arg;
    
	if (interrupt == NULL ||
		interrupt->size < HEADER_SIZE ||
		interrupt->size > MAX_NETWORK_PKT_SIZE) {

		free(interrupt);
		fprintf(stdout, "Bye!\n");
		return;
	}
    
    // check routing_packet_type
    // if route discovery either rebroadcast or send reply
    // if reply either forward or update cache
    
    routeheader = (routing_header_t) interrupt->buffer;
    routing_type = routeheader->routing_packet_type;
    printf("Routing type %d\n", routing_type);
	level = set_interrupt_level(DISABLED);
    if (routing_type) {
        miniroute_helper(interrupt);
        set_interrupt_level(level);
    } else {
        // return if packet was data packet not meant for me
		printf("forward %d\n",forward_packet(interrupt));
        if (forward_packet(interrupt)) return;
        
        // Find the protocol first
        dataheader = (mini_header_t) (interrupt->buffer + sizeof(struct routing_header));
        receiver_port = unpack_unsigned_short(dataheader->destination_port);
		printf("Receiver port%d\n", receiver_port);
        protocol = dataheader->protocol;
		printf("Protocol %d\n", protocol);
        
        //header and port validations
        if ((protocol == PROTOCOL_MINIDATAGRAM && (receiver_port < UNBOUND_MIN || receiver_port > UNBOUND_MAX)) ||
			(protocol == PROTOCOL_MINISTREAM && (receiver_port < 0 || receiver_port > SOCKET_SERVER_MAX))) {
            free(interrupt);
			printf("Oh no!\n");
            return;
        }
        
        //packet clean; deliver to appropriate protocol
        if (protocol == PROTOCOL_MINISTREAM) {
            minisocket_handle_incoming_packet(receiver_port, interrupt);
            set_interrupt_level(level);
        } else if (protocol == PROTOCOL_MINIDATAGRAM) {
			printf("Got datagram\n");
            port = miniport_create_unbound(receiver_port);
            
            miniport_unbound_enqueue(port, interrupt);
            set_interrupt_level(level);
        }
    }
}

void disk_handler(void* arg) {
    disk_interrupt_arg_t* disk_interrupt;
    
    disk_interrupt = (disk_interrupt_arg_t*)arg;
    printf("\nI don't do anything. You should know a disk interrupt happened");
}

/* minithread functions */

/*
 * The create function allocates and initializes stack for the new thread.
 */
minithread_t
minithread_create(proc_t proc, arg_t arg) {
	minithread_t new_thread;

    new_thread = (minithread_t)malloc(sizeof(struct minithread));
    if (NULL == new_thread) {
        fprintf(stderr, "NO MEMORY!!!");
		return NULL;
    }
    minithread_allocate_stack(&(new_thread -> stackbase), &(new_thread -> stacktop));
	minithread_initialize_stack(&(new_thread -> stacktop), proc, arg, &finalproc, (arg_t)new_thread);
    // initialize with lowest priority possible
	new_thread->priority = 0;
	new_thread->status = OK;
    return new_thread;
}

/*
 * the fork function creates a new thread i.e. allocates it a stack and puts it on the ready queue.
 */
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
	minithread_t thread;

    thread = minithread_create(proc, arg);
	minithread_start(thread);
    return thread;
}

minithread_t
minithread_self() {
    minithread_t self;
    interrupt_level_t level;

    level = set_interrupt_level(DISABLED);
    self = active_thread;
    set_interrupt_level(level);
    return self;
}

int
minithread_id() {
    return minithread_self()->thread_id;
}

int
minithread_priority(minithread_t thread) {
	return thread->priority;
}

/* DEPRECATED. Beginning from project 2, you should use minithread_unlock_and_stop() instead
 * of this function.
 */
void
minithread_stop() {
}

void
minithread_start(minithread_t t) {
    if(t -> status == SLEEPING) {
        t -> status = OK;
    }
    if(scheduler_schedule(t) == -1) {
        fprintf(stderr, "Could not start minithread\n");
    }
}

/*
 * The yield function appends the currently exectuting thread into the ready queue and 
 * starts execution of  the next thread in the ready queue 
 */
void
minithread_yield() {
    minithread_t running_thread, next_thread;
	int priority;
    interrupt_level_t level;

    level = set_interrupt_level(DISABLED);
    running_thread = minithread_self();
    priority = running_thread->priority;
    if (scheduler_quota_expired()) {
		// demote
        running_thread->priority = priority + 1 > 3 ? 3 : priority + 1;
    } else {
		// promote
        running_thread->priority = priority - 1 > 0 ? priority - 1 : 0;
    }
    if(scheduler_schedule(running_thread) == -1) {
        fprintf(stderr, "Cannot schedule minithread onto ready queue\n");
    }

    next_thread = scheduler_next_thread();
    active_thread = next_thread;
    
    minithread_switch(&running_thread->stacktop, &next_thread->stacktop);
    
    set_interrupt_level(level);
}

/*
 * minithread_unlock_and_stop(tas_lock_t* lock)
 *	Atomically release the specified test-and-set lock and
 *	block the calling thread.
 */
void
minithread_unlock_and_stop(tas_lock_t* lock) {
    if(lock != NULL) {
        atomic_clear(lock);
    }
	minithread_self()->status = SLEEPING;
    minithread_yield();
}

/*
 * sleep with timeout in milliseconds
 */
void
minithread_sleep_with_timeout(int delay) {
    interrupt_level_t level;
    semaphore_t sleeping = semaphore_create();
    level = set_interrupt_level(DISABLED);
    register_alarm(delay, (proc_t)&minithread_wake, (arg_t)sleeping);
    semaphore_P(sleeping);
    set_interrupt_level(level);
}

/*
 * Initialization.
 *
 * 	minithread_system_initialize:
 *	 This procedure should be called from your C main procedure
 *	 to turn a single threaded UNIX process into a multithreaded
 *	 program.
 *
 *	 Initialize any private data structures.
 * 	 Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 * 	 Start scheduling.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg) {
	minithread_t main_thread, idle_thread;
    network_address_t addr;
    disk_t* disk;
    
    disk = (disk_t*) malloc(sizeof(disk_t));
    if(disk == NULL) {
        fprintf(stderr, "OUT OF MEMORY");
        exit(-1);
    }
    
    idle_thread = (minithread_t) malloc(sizeof(struct minithread));
	idle_thread -> thread_id = 0;
	idle_thread -> stackbase = NULL;
	idle_thread -> stacktop = NULL;
	idle_thread -> priority = 0;
	idle_thread -> status = IDLE;
    
    //initialize ready and delete queues
	scheduler_initialize();
	scheduler_set_idle(idle_thread);
	delete_queue = queue_new();
    alarm_queue = init_alarm_queue();
    
    if (NULL == delete_queue || NULL == alarm_queue) {
        fprintf(stderr, "OUT OF MEMORY!\n");
        exit(-1);
    }
    
    schedule_reaper_thread = semaphore_create();
    alarm_sema = semaphore_create();
    
    minimsg_initialize();
	printf("Disk creation status: %d\n", disk_initialize(disk));

	//create global system threads
    main_thread = minithread_create(mainproc, mainarg);
	reaper_thread = minithread_fork(reap_proc, NULL);
    alarm_thread = minithread_fork(alarm_proc, NULL);
    
    active_thread = main_thread;
    
    minithread_clock_init(&clock_handler);
	//network_initialize(&network_handler);
    install_disk_handler(&disk_handler);
	//miniroute_initialize();
	
    minithread_switch(&(idle_thread -> stacktop), &(main_thread -> stacktop));
    
	while(1) {
        minithread_yield();
    }
}

/* Background thread functions */

/*
 * finalproc is executed once the mainproc of a thread finishes.
 * in finalproc the thread is appended on to the delete queue and
 * the context is switched into the reaper minithread who cleans this up.
 */
int
finalproc(arg_t final) {
	minithread_t thread;
    interrupt_level_t level;
    
    thread = minithread_self();
	thread->status = DESTROYED;
    assert(thread != NULL);
    level = set_interrupt_level(DISABLED);
    assert(queue_append(delete_queue, thread) != -1) ;
    set_interrupt_level(level);
    //signal the reaper thread to schedule
    semaphore_V(schedule_reaper_thread);
    
	minithread_yield();

    //Code never reached
    fprintf(stdout, "code not reached\n");
    return 0;
}

int
trigger_alarm(void* item, void* alarm_queue_item) {
    if(ticks == alarm_get_delay((alarm_item_t)alarm_queue_item)) {
        semaphore_V(alarm_sema);
		return 0;
    }
	return -1;
}

int
alarm_proc(int* arg) {
    alarm_item_t spawn_alarm;
	interrupt_level_t level;
    while(1) {
        level = set_interrupt_level(DISABLED);
        semaphore_P(alarm_sema);
        //TODO make wrapper to dequeue the queue
        assert(queue_dequeue(alarm_queue, (void **)&spawn_alarm) != 1);
        set_interrupt_level(level);
        alarm_get_func(spawn_alarm)(alarm_get_func_arg(spawn_alarm));
    }
}

void
minithread_wake(semaphore_t sleeping) {
	interrupt_level_t l;

	l = set_interrupt_level(DISABLED);
    semaphore_V(sleeping);
	set_interrupt_level(l);
    semaphore_destroy(sleeping);
}

/*
 * The reap_proc is the process called when the reaper_thread starts executing.
 * It manages the delete_queue, in that, it removes stale threads by freeing their stack and 
 * the pointers. Then it switches back to the idle thread once it is done with reaping.
 */
int
reap_proc(int* arg) {
    minithread_t item;
    interrupt_level_t level;

    while(1) {
        semaphore_P(schedule_reaper_thread);
        level = set_interrupt_level(DISABLED);
        if(queue_dequeue(delete_queue, (void **)&item) == -1) {
            fprintf(stderr, "Could not remove item from delete queue\n");
        }
        set_interrupt_level(level);
        assert(item!= NULL);
        minithread_free_stack(item->stackbase);
        free(item);
    }
}

#include <stdio.h>

#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "queue.h"

queue_t alarm_queue;
int gen_alarm_id;

struct alarm_item {
    int alarm_id;
	long delay;
    proc_t alarm_func;
	arg_t alarm_func_arg;
};

/*
 * insert alarm event into the alarm queue
 * returns an "alarm id", which is an integer that identifies the
 * alarm.
 */
int
register_alarm(int delay, proc_t func, arg_t arg) {
    alarm_item_t alarm;
    if(NULL == func || delay < 0) {
        return -1;
    }
    alarm = (alarm_item_t)malloc(sizeof(struct alarm_item));
    if(NULL == alarm) {
        return -1;
    }
    alarm -> alarm_func = func;
    //convert delay in millisec to ticks
    alarm -> delay = (long)((double)delay/(double)(PERIOD/MILLISECOND)) + ticks;
    
    //Failure case: alarm is missed if delay = ticks 
    if(alarm -> delay == ticks) {
        alarm -> delay += 1; 
    }
    alarm -> alarm_func_arg = arg;
    alarm -> alarm_id = gen_alarm_id++;
    queue_insert_sorted(alarm_queue, &compare_alarms, alarm);
    return alarm -> alarm_id;
}

int
compare_alarms(void* item1, void* item2) {
    if(((alarm_item_t)item1) -> delay < ((alarm_item_t)item2) -> delay) {
        return -1;
    } else if( ((alarm_item_t)item1) -> delay == ((alarm_item_t)item2) -> delay) {
        return 0;
    } else {
        return 1;
    }
}

int
delete_by_id_predicate(void* arg1, void* arg2) {
	if (((alarm_item_t)arg1)->alarm_id == (int)arg2) {
		return 0;
	}
	return -1;
}

/*
 * delete a given alarm  
 * it is ok to try to delete an alarm that has already executed.
 * The caller to deregister_alarm must ensure that the alarm element func_arg is freed.
 */
void
deregister_alarm(int alarmid) {
	alarm_item_t alarm_item;
	alarm_item = (alarm_item_t)queue_delete_by_predicate(alarm_queue, &delete_by_id_predicate, (void *)alarmid);
    free(alarm_item);
}

queue_t
init_alarm_queue() {
    gen_alarm_id = 0;
    alarm_queue = queue_new();
    if(NULL == alarm_queue) {
        return NULL;
    }
    return alarm_queue;
}

long
alarm_get_delay(alarm_item_t alarm) {
    return alarm -> delay;
}

int
alarm_set_delay(alarm_item_t alarm, long delay) {
    if(NULL == alarm) {
        return -1;
    }
    alarm -> delay = delay;
    return 0;
}

proc_t
alarm_get_func(alarm_item_t alarm) {
    return alarm -> alarm_func;
}

arg_t
alarm_get_func_arg(alarm_item_t alarm) {
    return alarm -> alarm_func_arg;
}


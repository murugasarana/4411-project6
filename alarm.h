#ifndef __ALARM_H_
#define __ALARM_H_

/*
 * This is the alarm interface. You should implement the functions for these
 * prototypes, though you may have to modify some other files to do so.
 */

/* register an alarm to go off in "delay" milliseconds, call func(arg) */
#include "queue.h"
#include "machineprimitives.h"

typedef struct alarm_item *alarm_item_t;

int register_alarm(int delay, proc_t func, arg_t arg);

void deregister_alarm(int alarmid);

queue_t init_alarm_queue();

int compare_alarms(void* item1, void* item2);

long alarm_get_delay(alarm_item_t alarm);
int alarm_set_delay(alarm_item_t alarm, long delay);
proc_t alarm_get_func(alarm_item_t alarm);
arg_t alarm_get_func_arg(alarm_item_t alarm);


#endif __ALARM_H_

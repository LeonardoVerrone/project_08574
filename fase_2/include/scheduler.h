#ifndef SCHEDULER_HEADER
#define SCHEDULER_HEADER

#include "types.h"

extern pcb_t* current_process;
extern struct list_head ready_queue;

void schedule();

void reloadIntervalTimer();
void reloadTimeslice();

#endif // !SCHEDULER_HEADER

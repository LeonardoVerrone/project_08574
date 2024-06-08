#ifndef SCHEDULER_HEADER
#define SCHEDULER_HEADER

#include "types.h"

extern pcb_t *current_process;
extern struct list_head ready_queue;

void schedule(pcb_t *pcb);
void updateCpuTime(pcb_t *pcb);

void reloadIntervalTimer();
void reloadTimeslice();

#endif // !SCHEDULER_HEADER

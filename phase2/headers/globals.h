#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

#include "devices.h"

#include <listx.h>
#include <types.h>

extern int process_count;
extern int soft_block_count;

extern pcb_t *ssi_pcb;
extern pcb_t *current_process;

extern struct list_head ready_queue;

// PCBs waiting for IO
extern pcb_t *waiting_for_IO[NUMBER_OF_DEVICES];

// PCBs waiting for Pseudo-clock
extern struct list_head waiting_for_PC;

// PCBs waiting got messages
extern struct list_head waiting_for_msg;

extern struct list_head pcbFree_h;

void init_globals();

#endif // GLOBALS_HEADER

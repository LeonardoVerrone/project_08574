#ifndef COMMON_HEADER
#define COMMON_HEADER

#include <types.h>

support_t *get_support_data();

pcb_t *create_process(state_t *, support_t *);

void terminate_process(pcb_t *);

unsigned int do_io(unsigned int *, unsigned int);

int get_missing_page(unsigned int);

void acquire_swap_mutex();
void release_swap_mutex();

#endif

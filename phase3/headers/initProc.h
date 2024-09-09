#ifndef INIT_PROC_HEADER
#define INIT_PROC_HEADER

#include <types.h>

// #define IEPBITON 0x4
// #define KUPBITON 0x8
// #define KUPBITOFF 0xFFFFFFF7
// #define TEBITON 0x08000000
// #define CAUSEINTMASK 0xFD00

extern pcb_t *sup_init_pcb;
extern pcb_t *swap_mutex;

void p3test();

pcb_t *create_process(state_t *, support_t *);

#endif // !INIT_PROC_HEADER

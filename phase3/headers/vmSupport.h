#ifndef VM_SUPPORT_HEADER
#define VM_SUPPORT_HEADER

#include <const.h>
#include <types.h>

// TODO: serve sta roba?
#define ENTRYHI_INIT 0x00000FC0 // no VPN, ASID set to -1
#define ENTRYLO_INIT 0x00000500 // no PFN, D-bit and G-bit up

#define SWAP_POOL_SIZE 2 * UPROCMAX

extern pcb_t *swap_mutex;

void initSwapStructs();

void swapMutexHandler();
void TLB_ExceptHandler();

#endif // !VM_SUPPORT_HEADER

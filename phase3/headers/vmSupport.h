#ifndef VM_SUPPORT_HEADER
#define VM_SUPPORT_HEADER

#include <const.h>
#include <types.h>

#define SWAP_POOL_BASE 0x20020000
#define SWAP_POOL_SIZE 2 * UPROCMAX

#define SW_ENTRY_UNUSED -1

extern pcb_t *sw_mutex_pcb;
extern pcb_t *mutex_holder;

void initSwapStructs();

void invalidate_uproc_frames(int);

void swapMutexHandler();
void TLB_ExceptHandler();

#endif // !VM_SUPPORT_HEADER

#include "scheduler.h"
#include "const.h"
#include "pcb.h"
#include <umps/cp0.h>
#include <umps/libumps.h>

extern int process_count;
extern int soft_block_count;
extern pcb_t *current_process;

void reloadIntervalTimer() { LDIT(PSECOND); }

void reloadTimeslice() { setTIMER(TIMESLICE); }

void schedule() {
  current_process = removeProcQ(&ready_queue);
  if (current_process) {
    reloadTimeslice();
    LDST(&current_process->p_s);
  } else {
    if (process_count == 1) {
      // TODO: controllo se l'unico che c'è è SSI
      HALT();
    } else if (process_count >= 1 && soft_block_count >= 1) {
      setSTATUS(STATUS_IEc | STATUS_IM_MASK);
      WAIT();
    } else {
      bp();
      PANIC();
    }
  }
}

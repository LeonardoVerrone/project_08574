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
  if (current_process == NULL) {
    if (process_count == 1) {
      // TODO: controllo se l'unico che c'è è SSI
      HALT();
    } else if (process_count > 1 && soft_block_count > 1) {
      // NOTE: codice molto debole, capace che fallisca.
      // TODO: controllare se serve la verifica di Kernel mode, o CU[0]=1
      setSTATUS(STATUS_IEc | STATUS_IM_MASK);
      WAIT();
    } else {
      PANIC();
    }
  } else {
    reloadTimeslice();
    LDST(&(current_process->p_s));
  }
}

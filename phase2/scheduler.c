#include "headers/scheduler.h"

#include "../phase1/headers/pcb.h"
#include "headers/globals.h"
#include "headers/util.h"

#include <types.h>

#include <umps/const.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

unsigned long start_timer;

void reloadIntervalTimer() { LDIT(PSECOND); }

void reloadTimeslice() { setTIMER(TIMESLICE); }

void updateCpuTime(pcb_t *pcb) {
  unsigned long cur_time;
  STCK(cur_time);
  pcb->p_time += cur_time - start_timer;
}

void schedule(pcb_t *pcb) {
  if (pcb == NULL) {
    current_process = removeProcQ(&ready_queue);
  } else {
    current_process = pcb;
  }

  if (current_process) {
    STCK(start_timer);
    if (pcb == NULL) {
      reloadTimeslice();
    }
    LDST(&current_process->p_s);
  } else {
    if (process_count == 1 && is_pcb_waiting(ssi_pcb)) {
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

/*
 * TODO: capire dove spostare questo metodo
 */
int is_pcb_waiting(pcb_t *p) {
  if (contains_pcb(&waiting_for_PC, p)) {
    return TRUE;
  }
  if (contains_pcb(&waiting_for_msg, p)) {
    return TRUE;
  }
  for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
    if (waiting_for_IO[i] == p)
      return TRUE;
  }
  return FALSE;
}

#include "scheduler.h"
#include "const.h"
#include "pcb.h"
#include <umps/const.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern int process_count;
extern int soft_block_count;
extern pcb_t *current_process;
extern pcb_t *ssi_pcb;

extern void klog_print_hex();
extern void klog_print_dec();
extern void klog_print();

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
    // TODO: spostare is_pcb_waiting
    // if (process_count == 1 && is_pcb_waiting(ssi_pcb)) {
    if (process_count == 1) {
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

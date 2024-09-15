#include "headers/scheduler.h"

#include "../phase1/headers/pcb.h"
#include "headers/globals.h"
#include "headers/util.h"

#include <types.h>

#include <umps/const.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

unsigned long start_timer;
static void reloadTimeslice();

/*
 * Funzione che implementa il comportamento dello scheduler. Alla chiamata viene
 * passato il puntatore del processo di cui fare lo schedule, se il puntatore è
 * NULL allora prendo il processo di cui fare lo schedule dalla ready_queue.
 */
void schedule(pcb_t *pcb) {
  if (pcb == NULL) {
    current_process = removeProcQ(&ready_queue);
  } else {
    current_process = pcb;
  }

  /*
   * Se ho un processo di cui fare lo schedule lo faccio
   */
  if (current_process) {
    STCK(start_timer);
    /* Aggiorno la timeslice solo se current_process è stato preso dalla
     * ready_queue */
    if (pcb == NULL) {
      reloadTimeslice();
    }
    LDST(&current_process->p_s);
  }

  /*
   * Vero quando l'unico processo esistente è quello dell'SSI, e si trova in
   * stato di attesa
   */
  if (process_count == 1 && is_pcb_waiting(ssi_pcb)) {
    HALT();
  }

  /*
   * Vero quando tutti i processi sono in attesa dello Pseudoclock o del
   * completamento di un'operazione IO
   */
  if (process_count >= 1 && soft_block_count >= 1) {
    setSTATUS(STATUS_IEc | STATUS_IM_MASK);
    WAIT();
  }

  PANIC();
}

/*
 * Funzione che aggiorna il tempo di CPU del processore viene fatta la
 * differenza tra l'istante attuale con quello salvato quando è stato fatto lo
 * schedule del processo.
 */
void updateCpuTime(pcb_t *pcb) {
  unsigned long cur_time;
  STCK(cur_time);
  pcb->p_time += cur_time - start_timer;
}

/*
 * Funzione che aggiorna lo Pseudoclock
 */
void reloadIntervalTimer() { LDIT(PSECOND); }

/*
 * Funzione che aggiorna il Processor Local Timer. Il valore TIMESLICE va
 * moltiplicato per il valore Time Scale, ovvero quanti tick il processore
 * esegue per microsecondo.
 */
static void reloadTimeslice() { setTIMER(TIMESLICE * (*(int *)TIMESCALEADDR)); }

/*
 * Restituisce TRUE sse il processo si trova in attesa dello Pseudoclock, o in
 * attesa di un messagio, o in attesa del completamento di un'operazione IO.
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

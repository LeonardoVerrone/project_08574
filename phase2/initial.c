#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "../phase3/headers/common.h"

#include "headers/exceptions.h"
#include "headers/globals.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"
#include "headers/util.h"

#include <const.h>
#include <types.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern pcb_t *test_pcb;
extern void p3test();

/*
 * Funzione che gestisce gli eventi TLB-Refill. Questi avvengono durante la
 * traduzione di un indirizzo logico non viene trovata una corrispondenza
 * all'interno del TLB. Il gestore inserisce nel TLB l'entry della pagina
 * richiesta cercandola nella Page Table del processo stesso:
 */
void uTLB_RefillHandler() {
  state_t *excp_state = (state_t *)BIOSDATAPAGE;
  support_t *excpt_sup = current_process->p_supportStruct;

  pteEntry_t *tlb_entry =
      &excpt_sup->sup_privatePgTbl[get_missing_page(excp_state->entry_hi)];

  setENTRYHI(tlb_entry->pte_entryHI);
  setENTRYLO(tlb_entry->pte_entryLO);
  TLBWR();
  LDST(excp_state);
}

int main() {
  {
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK;
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = KERNELSTACK;
  }

  initPcbs();
  initMsgs();

  init_globals();

  reloadIntervalTimer();

  // init ssi
  ssi_pcb = allocPcb();
  ssi_pcb->p_s.status = STATUS_IEp | STATUS_IM_MASK; // enables interrupts
  RAMTOP(ssi_pcb->p_s.reg_sp);
  ssi_pcb->p_s.reg_sp -= PAGESIZE;
  ssi_pcb->p_s.pc_epc = ssi_pcb->p_s.reg_t9 = (memaddr)SSI_handler;
  process_count++;

  insertProcQ(&ready_queue, ssi_pcb);

  test_pcb = allocPcb();
  test_pcb->p_s.status = STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  test_pcb->p_s.reg_sp = ssi_pcb->p_s.reg_sp - PAGESIZE;
  test_pcb->p_s.pc_epc = test_pcb->p_s.reg_t9 = (memaddr)p3test;
  process_count++;

  insertProcQ(&ready_queue, test_pcb);

  schedule(NULL);

  return 0;
}

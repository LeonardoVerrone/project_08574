#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "../phase3/headers/initProc.h"
#include "const.h"
#include "headers/exceptions.h"
#include "headers/globals.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"
#include "headers/util.h"

#include <types.h>
#include <umps/const.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern void klog_print(char *);
extern void klog_print_hex(unsigned int);
extern void klog_print_dec(int);

void uTLB_RefillHandler() {
  state_t *saved_excp_state = (state_t *)BIOSDATAPAGE;
  support_t *sup = current_process->p_supportStruct;

  unsigned int missing_page = saved_excp_state->entry_hi >> VPNSHIFT;

  pteEntry_t *tlb_entry = NULL;
  for (int i = 0; i < USERPGTBLSIZE && tlb_entry == NULL; i++) {
    if (missing_page == (sup->sup_privatePgTbl[i].pte_entryHI >> VPNSHIFT)) {
      tlb_entry = &(sup->sup_privatePgTbl[i]);
    }
  }

  setENTRYHI(tlb_entry->pte_entryHI);
  setENTRYLO(tlb_entry->pte_entryLO);
  TLBWR();
  LDST(saved_excp_state);
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

  sup_init_pcb = allocPcb();
  sup_init_pcb->p_s.status = STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  sup_init_pcb->p_s.reg_sp = ssi_pcb->p_s.reg_sp - PAGESIZE;
  sup_init_pcb->p_s.pc_epc = sup_init_pcb->p_s.reg_t9 = (memaddr)p3test;
  process_count++;

  insertProcQ(&ready_queue, sup_init_pcb);

  schedule(NULL);

  return 0;
}

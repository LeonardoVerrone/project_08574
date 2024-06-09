#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/exceptions.h"
#include "headers/globals.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"

#include <types.h>

#include <umps/const.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern void test();

void uTLB_RefillHandler() {
  setENTRYHI(0x80000000);
  setENTRYLO(0x00000000);
  TLBWR();
  LDST((state_t *)0x0FFFF000);
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
  RAMTOP(ssi_pcb->p_s.reg_sp); // stack pointer points to RAMTOP
  ssi_pcb->p_s.pc_epc = ssi_pcb->p_s.reg_t9 = (memaddr)SSI_handler;
  process_count++;

  insertProcQ(&ready_queue, ssi_pcb);

  pcb_t *test_pcb = allocPcb();
  test_pcb->p_s.status |= STATUS_IEc | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  RAMTOP(test_pcb->p_s.reg_sp);
  test_pcb->p_s.reg_sp -= 2 * PAGESIZE;
  test_pcb->p_s.pc_epc = test_pcb->p_s.reg_t9 = (memaddr)test;
  process_count++;

  insertProcQ(&ready_queue, test_pcb);

  schedule(NULL);

  return 0;
}

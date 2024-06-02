#include "exceptions.h"
#include "msg.h"
#include "pcb.h"
#include "scheduler.h"
#include "ssi.h"

#include <umps/cp0.h>
#include <umps/libumps.h>

int process_count;
int soft_block_count;
pcb_t *current_process;
// blocked PCBs

pcb_t *ssi_pcb;

extern void test();

struct list_head ready_queue;

// extern void test();

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

  process_count = 0;
  soft_block_count = 0;
  current_process = NULL;
  mkEmptyProcQ(&ready_queue);

  // TODO: init blocked PCBs

  reloadIntervalTimer();

  // init ssi
  ssi_pcb = allocPcb();
  ssi_pcb->p_s.status |= STATUS_IEp | STATUS_IM_MASK; // enables interrupts
  ssi_pcb->p_s.status |= STATUS_KUp;                  // kernel mode on
  RAMTOP(ssi_pcb->p_s.reg_sp); // stack pointer points to RAMTOP
  ssi_pcb->p_s.pc_epc = ssi_pcb->p_s.reg_t9 = (memaddr)SSI_handler;

  insertProcQ(&ready_queue, ssi_pcb);
  process_count++;

  pcb_t *second_p = allocPcb();
  second_p->p_s.status |= STATUS_IEp | STATUS_IM_MASK;
  second_p->p_s.status |= STATUS_KUp;
  second_p->p_s.status |= STATUS_TE; // enables Local Timer
  RAMTOP(second_p->p_s.reg_sp) - (2 * PAGESIZE);
  second_p->p_s.pc_epc = second_p->p_s.reg_t9 = (memaddr)test;

  insertProcQ(&ready_queue, second_p);
  process_count++;

  schedule();

  return 0;
}

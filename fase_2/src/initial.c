#include "exceptions.h"

#include "klog.c"
#include "msg.h"
#include "pcb.h"
#include "scheduler.h"
#include "ssi.h"
#include "types.h"

#include <umps/const.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern void klog_print_dec();
extern void klog_print_hex();
extern void klog_print();

int process_count, soft_block_count;
pcb_t *current_process;
// blocked PCBs

pcb_t *ssi_pcb;

extern void test();

struct list_head ready_queue;

// TODO: to remove
ssi_payload_t *create_process_payload;
ssi_create_process_t *create_process_arg;

pcb_t *waiting_for_IO[NUMBER_OF_DEVICES];
struct list_head waiting_for_PC; // PCBs waiting for Pseudo-clock

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
  mkEmptyProcQ(&waiting_for_PC);
  for (int i = 0; i < NUMBER_OF_DEVICES; i++)
    waiting_for_IO[i] = NULL;

  reloadIntervalTimer();

  // init ssi
  ssi_pcb = allocPcb();
  ssi_pcb->p_s.status = STATUS_IEp | STATUS_IM_MASK; // enables interrupts
  RAMTOP(ssi_pcb->p_s.reg_sp); // stack pointer points to RAMTOP
  ssi_pcb->p_s.pc_epc = ssi_pcb->p_s.reg_t9 = (memaddr)SSI_handler;
  process_count++;

  insertProcQ(&ready_queue, ssi_pcb);

  pcb_t *second_p = allocPcb();
  second_p->p_s.status |= STATUS_IEc | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  RAMTOP(second_p->p_s.reg_sp);
  second_p->p_s.reg_sp -= 2 * PAGESIZE;
  second_p->p_s.pc_epc = second_p->p_s.reg_t9 = (memaddr)test;
  process_count++;

  insertProcQ(&ready_queue, second_p);

  initExceptionHandler();

  schedule(NULL);

  return 0;
}

#include "interrupts.h"
#include "listx.h"
#include "msg.h"
#include "pcb.h"
#include "scheduler.h"

#include <umps/arch.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

#define INTPROC_INT 0
#define PLT_INT 1
#define LOCALTIMER_INT 2
#define DISKDEV_INT 3
#define FLASHDEV_INT 4
#define NETDEV_INT 5
#define PRINTERDEV_INT 6
#define TERMDEV_INT 7

#define NUMBER_OF_INTLINES 8

extern int soft_block_count;
extern pcb_t *ssi_pcb;
extern struct list_head waiting_for_msg;
extern struct list_head waiting_for_PC;
extern pcb_t *waiting_for_IO[];
extern void klog_print_dec();
extern void klog_print_hex();
extern void klog_print();

int infer_intline() {
  for (int i = 0; i < NUMBER_OF_INTLINES; i++)
    if (getCAUSE() & CAUSE_IP(i))
      return i;
  return -1;
}

/*
 * WARN: funziona per term0, ma da testare con
 * altri device
 */
int infer_dev_number(int intline) {
  unsigned int *line_address = (unsigned int *)CDEV_BITMAP_ADDR(intline);
  for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
    if ((*(line_address) >> i) & 0x1) {
      return i;
    }
  }
  return -1;
}

void ack_subdevice(unsigned int status, unsigned int *command, int dev_idx) {
  int status_code = status & TERMSTATMASK;
  if (status_code == READY) {
    return;
  }

  if (status_code == TERM_OKSTATUS) {
    *command = ACK;
    pcb_t *caller = waiting_for_IO[dev_idx];
    waiting_for_IO[dev_idx] = NULL;
    if (caller != NULL) {
      caller->p_s.reg_v0 = status_code;
      msg_t *msg = allocMsg();
      if (msg != NULL) { // WARN: se msg == NULL non viene gestito
        msg->m_sender = ssi_pcb;
        msg->m_payload = status;
        insertMessage(&caller->msg_inbox, msg);
      }
      outProcQ(&waiting_for_msg, caller);
      insertProcQ(&ready_queue, caller);
      soft_block_count--;
      waiting_for_IO[dev_idx] = NULL;
    }
  } else {
    // TODO: come voglio gestire gli altri casi?
  }
}

void handle(int interrupt_line) {
  int dev_number = infer_dev_number(interrupt_line);

  if (interrupt_line != 7 || dev_number != 0) {
    // TODO: implementare comportamente per i rimanenti interrupt
    return;
  }
  termreg_t *term0_reg = (termreg_t *)DEV_REG_ADDR(interrupt_line, dev_number);

  int dev_idx = (interrupt_line - 3) * 8 + dev_number;
  ack_subdevice(term0_reg->recv_status, &term0_reg->recv_command, dev_idx);
  ack_subdevice(term0_reg->transm_status, &term0_reg->transm_command,
                dev_idx + 8);

  schedule(current_process);
}

// arrivo che ho già caricato lo stato di current_process
void interruptHandler() {
  int interrupt_line = infer_intline();
  if (interrupt_line == -1) {
    // TODO: gestione errore
    return;
  }
  switch (interrupt_line) {
  case INTPROC_INT:
    // ignoring
    break;
  case PLT_INT:
    insertProcQ(&(ready_queue), current_process);
    schedule(NULL);
    break;
  case LOCALTIMER_INT:
    pcb_t *iter;
    while ((iter = removeProcQ(&waiting_for_PC)) != NULL) {
      msg_t *msg = allocMsg();
      if (msg != NULL) {
        msg->m_sender = ssi_pcb;
        insertMessage(&iter->msg_inbox, msg);
        insertProcQ(&ready_queue, iter);
        soft_block_count--;
      }
    }
    reloadIntervalTimer();
    schedule(current_process);
    break;
  default: // Non timer Interrupts
    handle(interrupt_line);

    break;
  }
}

#include "headers/interrupts.h"

#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/devices.h"
#include "headers/globals.h"
#include "headers/scheduler.h"

#include <umps/arch.h>
#include <umps/libumps.h>
#include <umps/types.h>

extern void klog_print(char *str);
extern void klog_print_hex(unsigned int);
extern void klog_print_dec(int);

static void free_waitingIO_pcb(unsigned int status, int idx) {
  // ricavo il chiamante, e lo sblocco
  pcb_t *caller = waiting_for_IO[idx];
  if (caller == NULL) {
    return;
  }
  waiting_for_IO[idx] = NULL;

  caller->p_s.reg_v0 = status & TERMSTATMASK;
  msg_t *msg = allocMsg();
  if (msg == NULL) {
    PANIC();
  }

  // invio messaggio per sbloccare il chiamante
  msg->m_sender = ssi_pcb;
  msg->m_payload = status;
  insertMessage(&caller->msg_inbox, msg);

  // metto in chiamante nello stato 'ready'
  outProcQ(&waiting_for_msg, caller);
  insertProcQ(&ready_queue, caller);
  soft_block_count--;
}

/*
 * Esegue l'ack del comando specificato.
 * Si occupa sia di scrivere ACK nel registro del device in questione, che di
 * sbloccare il processo che era in attesa della risposta da parte del device.
 * della risposta da parte del device.
 */
static void ack_device_interrupts(device_id_t dev_id) {
  int dev_idx = dev_id.dev_class * DEVPERINT + dev_id.dev_number;
  unsigned int status;

  switch (dev_id.dev_class) {
  case DISK_CLASS:
  case FLASH_CLASS:
  case PRNT_CLASS:
    dtpreg_t *dtp_reg =
        (dtpreg_t *)compute_reg_address(dev_id.dev_class, dev_id.dev_number);
    ;
    status = dtp_reg->status;
    dtp_reg->command = ACK;
    free_waitingIO_pcb(status, dev_idx);
    break;
  case TERM_CLASS:
    termreg_t *termreg =
        (termreg_t *)compute_reg_address(dev_id.dev_class, dev_id.dev_number);

    /* ACK del subdevice RECV */
    status = termreg->recv_status;
    if ((status & TERMSTATMASK) != DEVSTATUS_READY) {
      *((unsigned int *)termreg->recv_command) = ACK;
      free_waitingIO_pcb(status, dev_idx);
    }

    /* ACK del subdevice TRANSM*/
    status = termreg->transm_status;
    if ((status & TERMSTATMASK) != DEVSTATUS_READY) {
      termreg->transm_command = ACK;
      free_waitingIO_pcb(status, dev_idx + 8);
    }
    break;
  case NETW_CLASS:
    // TODO: implementare comportamento
    break;
  default:
    PANIC();
    break;
  }
}

// arrivo che ho già caricato lo stato di current_process
void interruptHandler() {
  int interrupt_line = get_intline_from_cause();
  if (interrupt_line == -1) {
    schedule(NULL);
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
    device_id_t dev_id = {.dev_class = interrupt_line - 3,
                          .dev_number = get_dev_number(interrupt_line)};

    ack_device_interrupts(dev_id);

    schedule(current_process);
    break;
  }
}

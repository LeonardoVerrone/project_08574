#include "headers/interrupts.h"

#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/devices.h"
#include "headers/globals.h"
#include "headers/scheduler.h"

#include <umps/arch.h>
#include <umps/libumps.h>

/*
 * Esegue l'ack del comando specificato.
 * Si occupa sia di scrivere ACK nel registro del device in questione, che di
 * sbloccare il processo che era in attesa della risposta da parte del device.
 * della risposta da parte del device.
 *
 * NOTE: codice sviluppato per TERM0, nelle fasi successive adattare per i
 * device rimanenti
 */
static void ack_subdevice(unsigned int status, unsigned int *command,
                          int dev_idx) {
  int status_code = status & TERMSTATMASK;
  if (status_code == READY) { // non devo fare l'ack del subdevice
    return;
  }

  if (status_code == TERM_OKSTATUS) {
    *command = ACK; // abbasso interrupt

    // ricavo il chiamante, e lo sblocco
    pcb_t *caller = waiting_for_IO[dev_idx];
    waiting_for_IO[dev_idx] = NULL;
    if (caller == NULL) {
      return;
    }

    caller->p_s.reg_v0 = status_code;
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
}

void handle(int interrupt_line) {}

// arrivo che ho già caricato lo stato di current_process
void interruptHandler() {
  int interrupt_line = get_intline_from_cause();
  if (interrupt_line == -1) {
    PANIC();
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
    int dev_number = get_dev_number(interrupt_line);

    if (interrupt_line != TERMDEV_INT || dev_number != 0) {
      // TODO: nelle fasi successive implementare comportamento per i rimanenti
      // interrupt
      return;
    }

    // registro del terminale
    termreg_t *term0_reg =
        (termreg_t *)DEV_REG_ADDR(interrupt_line, dev_number);

    int dev_idx = (interrupt_line - 3) * 8 +
                  dev_number; // indice del device all'interno di waiting_for_IO
    ack_subdevice(term0_reg->recv_status, &term0_reg->recv_command,
                  dev_idx); // ack di RECV
    ack_subdevice(term0_reg->transm_status, &term0_reg->transm_command,
                  dev_idx + 8); // ack di TRANSM

    schedule(current_process);
    break;
  }
}

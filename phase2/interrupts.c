#include "headers/interrupts.h"

#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/devices.h"
#include "headers/globals.h"
#include "headers/scheduler.h"

#include <umps/libumps.h>

static void free_waitingIO_pcb(unsigned int, int);
static void ack_device_interrupts(device_id_t);

/*
 * Funzione che implementa il gestore degli interrupt. Viene chiamata dal
 * gestore delle eccezioni, pertanto il processore si trova in modalità kernel
 * con gli interrupt abbassati.
 */
void interruptHandler() {
  int interrupt_line = get_intline_from_cause();

  switch (interrupt_line) {
  /*
   * Interrupts inter processore, non faccio nulla per il momento.
   */
  case INTPROC_INT:
    break;

  /*
   * Processor Local Timer interrupt, il processo corrente ha esaurito il tempo
   * a lui dedicato per l'utilizzo della cpu. Deve quindi essere messo in stato
   * READY per poi fare un reschedule.
   */
  case PLT_INT:
    insertProcQ(&(ready_queue), current_process);
    schedule(NULL);
    break;

  /*
   * System-wide Local Timer, o Pseudoclock, è necessario risvegliare tutti i
   * processi che erano in attesa dello psudoclock.
   */
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

  /*
   * Non-timer interrupts, individuo il device che ha scatenato l'interrupt e
   * faccio l'ACK.
   */
  default:
    device_id_t dev_id = {.dev_class = interrupt_line - 3,
                          .dev_number = get_dev_number(interrupt_line)};

    ack_device_interrupts(dev_id);

    schedule(current_process);
    break;
  }
}

/*
 * Funzione che esegue l'ACK dell'interrupt scatenato dal device specificato.
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
        (dtpreg_t *)compute_devreg_addr(dev_id.dev_class, dev_id.dev_number);
    status = dtp_reg->status;
    dtp_reg->command = ACK;
    free_waitingIO_pcb(status, dev_idx);
    break;
  case TERM_CLASS:
    termreg_t *termreg =
        (termreg_t *)compute_devreg_addr(dev_id.dev_class, dev_id.dev_number);

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

/*
 * Funzione che risveglia il processo che stava attendendo il completamento
 * dell'operazione IO sul device che ha scatenato l'interrupt.
 * Viene ricavato il processo chiamante interrogando l'array 'waiting_for_IO'
 * presso l'indice del device che ha scatenato l'interrupt. Successivamente
 * viene risvegliato tale processo e gli viene inviato un messaggio contente lo
 * stato del device.
 */
static void free_waitingIO_pcb(unsigned int status, int idx) {

  /*
   * Ricavo il processo in attesa e lo risveglio
   */
  pcb_t *caller = waiting_for_IO[idx];
  if (caller == NULL) {
    PANIC();
  }
  waiting_for_IO[idx] = NULL;
  outProcQ(&waiting_for_msg, caller);
  insertProcQ(&ready_queue, caller);
  soft_block_count--;

  /*
   * Invio il messaggio e restituisco lo stato del device
   */
  msg_t *msg = allocMsg();
  if (msg == NULL) {
    PANIC();
  }
  msg->m_sender = ssi_pcb;
  msg->m_payload = status;
  insertMessage(&caller->msg_inbox, msg);
  caller->p_s.reg_v0 = status & TERMSTATMASK;
}

#include "headers/ssi.h"

#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/devices.h"
#include "headers/globals.h"
#include "headers/scheduler.h"
#include "headers/util.h"

#include <umps/libumps.h>

static void SSI_create_process(pcb_t *, ssi_create_process_t *);
static void SSI_doIO(pcb_t *, ssi_do_io_t *);
static void SSI_get_process_id(pcb_t *, void *);

/*
 * Funzione che implementa il comportamento dell'SSI: attende richieste da parte
 * degli altri processi per procedere ad eseguirle.
 */
void SSI_handler() {
  while (1) {
    ssi_payload_t *payload = NULL;
    pcb_t *sender =
        (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
    switch (payload->service_code) {
    case CREATEPROCESS:
      SSI_create_process(sender, (ssi_create_process_t *)payload->arg);
      break;
    case TERMPROCESS:
      if (payload->arg == NULL)
        kill_process(sender);
      else
        kill_process((pcb_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    case DOIO:
      SSI_doIO(sender, (ssi_do_io_t *)payload->arg);
      break;
    case GETTIME:
      SYSCALL(SENDMESSAGE, (unsigned int)sender, sender->p_time, 0);
      break;
    case CLOCKWAIT:
      /* NOTA: incremento il soft_block_count solo se il processo non ha ancora
       *inviato la richiesta RECEIVEMESSAGE
       */
      if (outProcQ(&waiting_for_msg, sender) == NULL) {
        soft_block_count++;
      }
      outProcQ(&ready_queue, sender);

      insertProcQ(&waiting_for_PC, sender);
      break;
    case GETSUPPORTPTR:
      SYSCALL(SENDMESSAGE, (unsigned int)sender,
              (unsigned int)sender->p_supportStruct, 0);
      break;
    case GETPROCESSID:
      SSI_get_process_id(sender, payload->arg);
      break;
    default:
      kill_process(sender);
      break;
    }
  }
}

/*
 * Funzione che termina il processo specificato: viene rimosso dalla gerarchia a
 * cui appartiene, viene rimosso dalle liste in attesa in cui è inserito, e
 * infine viene deallocato il pcb.
 */
void kill_process(pcb_t *p) {
  /*
   * Rimuovo il processo dalla gerarchia a cui appartiene: termino i figli e poi
   * lo rendo orfano (lo rimuovo dalla lista dei figli del genitore)
   */
  pcb_t *child;
  while ((child = removeChild(p)) != NULL) {
    kill_process(child);
  }
  outChild(p);

  if (is_pcb_waiting(p)) {
    soft_block_count--;
    // rimuovo il processo dalle liste in cui era bloccato
    for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
      if (waiting_for_IO[i] == p) {
        waiting_for_IO[i] = NULL;
      }
    }
    outProcQ(&waiting_for_PC, p);
    outProcQ(&waiting_for_msg, p);
  }

  process_count--;

  freePcb(p);
}

/*
 * Funzione che crea un nuovo processo avente come genitore il processo
 * specificato. Alla funzione viene passato il payload della richiesta SSI che
 * lo stato e la struttura di supporto del nuovo processo.
 */
static inline void SSI_create_process(pcb_t *parent,
                                      ssi_create_process_t *arg) {
  pcb_t *new_pcb = allocPcb();
  if (new_pcb == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)parent, (unsigned int)NOPROC, 0);
    return;
  }
  process_count++;

  memcpy(&(new_pcb->p_s), arg->state, sizeof(state_t));
  if (arg->support != NULL) {
    new_pcb->p_supportStruct = arg->support;
  }

  insertChild(parent, new_pcb);
  insertProcQ(&ready_queue, new_pcb);

  SYSCALL(SENDMESSAGE, (unsigned int)parent, (unsigned int)new_pcb, 0);
}

/*
 * Funzione che esegue una richiesta IO al device contenuto nel payload
 * specificato.
 */
static void SSI_doIO(pcb_t *sender, ssi_do_io_t *arg) {
  // partendo dal comando, ricavo la classe e il numero del device
  device_id_t device_id;
  get_device_id((unsigned int)arg->commandAddr, &device_id);

  // trovo l'indice della "lista" di attesa del device
  int dev_idx = device_id.dev_class * 8 + device_id.dev_number;
  if (device_id.dev_class == TERM_CLASS &&
      compute_devreg_addr(device_id.dev_class, device_id.dev_number) +
              3 * WORDLEN ==
          (unsigned int)
              arg->commandAddr) { // true iff the command is TRANSM command
    dev_idx += DEVPERINT;
  }

  // se il device e libero eseguo la richiesta, altrimenti mi metto "in attesa"
  if (waiting_for_IO[dev_idx] == NULL) {
    // se stava aspettando dei messaggi lo tolgo
    if (contains_pcb(&waiting_for_msg, sender)) {
      outProcQ(&waiting_for_msg, sender);
    } else {
      soft_block_count++;
      if (contains_pcb(&ready_queue, sender))
        outProcQ(&ready_queue, sender);
    }
    waiting_for_IO[dev_idx] = sender;

    *arg->commandAddr = arg->commandValue;
  } else {
    // se c'è già un altro pcb in attesa del device, mi metto "in attesa"
    // inviando una nuova richiesta DoIO all'ssi
    msg_t *msg = allocMsg();
    if (msg == NULL) {
      PANIC();
    }
    ssi_payload_t payload = {
        .service_code = DOIO,
        .arg = arg,
    };
    msg->m_sender = sender;
    msg->m_payload = (unsigned int)&payload;
    insertMessage(&ssi_pcb->msg_inbox, msg);
  }
}

/*
 * Funzione che restituisce invia al mittente il proprio pid
 */
static void SSI_get_process_id(pcb_t *sender, void *arg) {
  if (arg == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)sender, sender->p_pid, 0);
    return;
  }
  if (sender->p_parent == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
    return;
  }
  SYSCALL(SENDMESSAGE, (unsigned int)sender, sender->p_parent->p_pid, 0);
}

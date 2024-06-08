#include "ssi.h"
#include "const.h"
#include "exceptions.h"
#include "listx.h"
#include "msg.h"
#include "pcb.h"
#include "scheduler.h"
#include "types.h"
#include "util.h"

#include <umps/libumps.h>

extern int process_count, soft_block_count;
extern pcb_t *waiting_for_IO[NUMBER_OF_DEVICES];
extern struct list_head waiting_for_PC;
extern struct list_head waiting_for_msg;

unsigned int ra;

extern void klog_print_dec();
extern void klog_print_hex();
extern void klog_print();

typedef struct device_id {
  int class;
  int number;
} device_id_t;

static inline int is_pcb_waiting(pcb_t *p) {
  if (contains_pcb(&waiting_for_PC, p)) {
    return TRUE;
  }
  if (contains_pcb(&waiting_for_msg, p)) {
    return TRUE;
  }
  for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
    if (waiting_for_IO[i] == p)
      return TRUE;
  }
  return FALSE;
}

static inline void SSI_create_process(pcb_t *parent,
                                      ssi_create_process_t *arg) {
  pcb_t *new_pcb = allocPcb();
  if (new_pcb == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)parent, (unsigned int)NOPROC, 0);
    return;
  }
  process_count++;

  state_memcpy(&(new_pcb->p_s), arg->state);
  if (arg->support != NULL) {
    new_pcb->p_supportStruct = arg->support;
  }

  insertChild(parent, new_pcb);
  insertProcQ(&ready_queue, new_pcb);

  SYSCALL(SENDMESSAGE, (unsigned int)parent, (unsigned int)new_pcb, 0);
}

/*
 * Come umps/arch.h#DEV_REG_ADD(line, dev), ma con la classe al posto della
 * linea
 */
static unsigned int compute_reg_address(int dev_class, int dev_number) {
  return START_DEVREG + (dev_class * DEVREGSIZE * 8) +
         (dev_number * DEVREGSIZE);
}

static void infer_device_id(unsigned int command_address,
                            device_id_t *device_id) {
  // parto dal basso e scorro tutti i device fino a trovare quello
  // corrispondente al registro passato
  for (int dev_class = 0; dev_class < 5; dev_class++) {
    for (int dev_number = 0; dev_number < 8; dev_number++) {
      if ((dev_class, dev_number) <= command_address &&
          command_address <=
              compute_reg_address(dev_class, dev_number) + DEVREGSIZE) {
        device_id->class = dev_class;
        device_id->number = dev_number;
        return;
      }
    }
  }
  // FIXME: caso non gestito
}

static void SSI_doIO(pcb_t *sender, ssi_do_io_t *arg) {
  // partendo dal comando, ricavo la classe e il numero del device
  device_id_t device_id;
  infer_device_id((unsigned int)arg->commandAddr, &device_id);
  if (device_id.class != 4 || device_id.number != 0) {
    // TODO: implementare comportamente anche per gli altri device
    return;
  }

  // trovo l'indice della "lista" di attesa del device
  int dev_idx = device_id.class * 8 + device_id.number;
  if (compute_reg_address(device_id.class, device_id.number) + 3 * WORDLEN ==
      (unsigned int)
          arg->commandAddr) { // true iff the command is TRANSM command
    dev_idx += DEVPERINT;
  }
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
    msg_t *msg = allocMsg();
    if (msg == NULL) {
      // TODO: segnala errore
      return;
    }
    ssi_payload_t payload = {
        .service_code = DOIO,
        .arg = arg,
    };
    msg->m_sender = sender;
    msg->m_payload = (unsigned int)&payload;
  }
}

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

static void SSI_get_cpu_time(pcb_t *sender) {
  SYSCALL(SENDMESSAGE, (unsigned int)sender, sender->p_time, 0);
}

void kill_process(pcb_t *p) {
  // termino tutti i figli
  pcb_t *child;
  while ((child = removeChild(p)) != NULL) {
    kill_process(child);
  }

  // rendo il processo orfano
  outChild(p);

  if (is_pcb_waiting(p)) {
    soft_block_count--;
  }

  // se era in attesa di IO, lo rimuovo dalla
  for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
    if (waiting_for_IO[i] == p) {
      waiting_for_IO[i] = NULL;
    }
  }
  outProcQ(&waiting_for_PC, p);
  outProcQ(&waiting_for_msg, p);

  process_count--;

  freePcb(p);
}

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
      SSI_get_cpu_time(sender);
      break;
    case CLOCKWAIT:
      outProcQ(&waiting_for_msg, sender);
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

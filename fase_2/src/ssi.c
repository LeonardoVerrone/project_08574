#include "ssi.h"
#include "const.h"
#include "listx.h"
#include "pcb.h"
#include "scheduler.h"
#include "types.h"
#include "util.h"

#include <umps/libumps.h>
#include <umps/types.h>

extern int process_count, soft_block_count;
extern pcb_t *waiting_for_IO[NUMBER_OF_DEVICES];

unsigned int ra;

extern void klog_print_dec();
extern void klog_print_hex();
extern void klog_print();

typedef struct device_id {
  int class;
  int number;
} device_id_t;

static inline void SSI_create_process(pcb_t *parent,
                                      ssi_create_process_t *arg) {
  pcb_t *new_pcb = allocPcb();
  if (new_pcb == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)parent, (unsigned int)NOPROC, 0);
    return;
  }

  state_memcpy(&(new_pcb->p_s), arg->state);
  if (arg->support) {
    new_pcb->p_supportStruct = arg->support;
  }
  new_pcb->p_pid = ++process_count;
  new_pcb->p_time = 0;

  insertChild(parent, new_pcb);
  insertProcQ(&ready_queue, new_pcb);

  SYSCALL(SENDMESSAGE, (unsigned int)parent, (unsigned int)new_pcb, 0);
}

/*
 * Come umps/arch.h#DEV_REG_ADD(line, dev), ma con la classe al posto della
 * linea
 */
unsigned int compute_reg_address(int dev_class, int dev_number) {
  return START_DEVREG + (dev_class * DEVREGSIZE * 8) +
         (dev_number * DEVREGSIZE);
}

void infer_device_id(unsigned int command_address, device_id_t *device_id) {
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

void SSI_doIO(pcb_t *sender, ssi_do_io_t *arg) {
  device_id_t device_id;
  infer_device_id((unsigned int)arg->commandAddr, &device_id);
  if (device_id.class != 4 || device_id.number != 0) {
    // TODO: implementare comportamente anche per gli altri device
    return;
  }

  // scelgo la lista del subdevice (RECV o TRANSM)
  int dev_idx = device_id.class * 8 + device_id.number;
  if (compute_reg_address(device_id.class, device_id.number) + 3 * WORDLEN ==
      (unsigned int)
          arg->commandAddr) { // true iff the command is TRANSM command
    dev_idx += DEVPERINT;
  }
  if (waiting_for_IO[dev_idx] != NULL) {
    // TODO: come gestisco errore?
    klog_print("errore");
    return;
  }
  waiting_for_IO[dev_idx] = sender;
  klog_print_dec(dev_idx);
  klog_print(", ");
  soft_block_count++;

  *arg->commandAddr = arg->commandValue;
}

void SSI_get_process_id(pcb_t *sender, void *arg) {
  if (arg == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)sender->p_pid, 0);
    return;
  }
  if (sender->p_parent == NULL) {
    SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
    return;
  }
  SYSCALL(SENDMESSAGE, (unsigned int)sender->p_parent, 0, 0);
}

void SSI_get_cpu_time(pcb_t *sender) {
  SYSCALL(SENDMESSAGE, (unsigned int)sender, sender->p_time, 0);
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
      break;
    case DOIO:
      SSI_doIO(sender, (ssi_do_io_t *)payload->arg);
      break;
    case GETTIME:
      SSI_get_cpu_time(sender);
      break;
    case CLOCKWAIT:
      break;
    case GETSUPPORTPTR:
      break;
    case GETPROCESSID:
      SSI_get_process_id(sender, payload->arg);
      break;
    default:
      // TODO: terminate
      bp();
    }
  }
}

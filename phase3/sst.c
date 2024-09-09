#include "headers/sst.h"

#include "../phase2/headers/devices.h"
#include "const.h"
#include <types.h>

#include <umps/cp0.h>
#include <umps/libumps.h>
#include <umps/types.h>

extern pcb_t *ssi_pcb;
extern pcb_t *sup_init_pcb;

extern void klog_print(char *str);
extern void klog_print_hex(unsigned int);

extern pcb_t *create_process(state_t *, support_t *);
extern support_t *get_support_data();

extern void bp();

void print(device_id_t dev_id, sst_print_t *print_payload) {
  devreg_t *devreg =
      (devreg_t *)compute_reg_address(dev_id.dev_class, dev_id.dev_number);
  for (int i = 0; i < print_payload->length; i++) {
    ssi_do_io_t do_io;
    switch (dev_id.dev_class) {
    case TERM_CLASS:
      termreg_t *termreg = &devreg->term;
      do_io.commandAddr = &termreg->transm_command;
      do_io.commandValue = TRANSMITCHAR | (print_payload->string[i] << 8);
      break;
    case PRNT_CLASS:
      dtpreg_t *dtpreg = &devreg->dtp;
      dtpreg->data0 = print_payload->string[i];
      do_io.commandAddr = &dtpreg->command;
      do_io.commandValue = PRINTCHAR;
      break;
    default:
      PANIC();
    }
    unsigned int status;
    ssi_payload_t payload = {.service_code = DOIO, .arg = &do_io};
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&status, 0);
  }
}

void sst_handler() {
  /*
   * Creo U-PROC figlio
   */
  /* Init dello stato */
  state_t uproc_state;
  STST(&uproc_state); // in particolare memorizzo l'EntryHi.ASID dall'SST
  uproc_state.reg_sp = USERSTACKTOP;
  uproc_state.pc_epc = uproc_state.reg_t9 = UPROCSTARTADDR;
  uproc_state.status =
      ALLOFF | STATUS_KUp | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;

  /* Recupero la Support Structure da quella dell'SST */
  support_t *uproc_support = get_support_data();

  pcb_t *uproc_pcb = create_process(&uproc_state, uproc_support);
  klog_print(", UPROC[");
  klog_print_hex(uproc_support->sup_asid);
  klog_print("] pcb:");
  klog_print_hex((unsigned int)uproc_pcb);

  while (TRUE) {
    device_id_t dev_id;
    /**
     * Receive message and elaborate
     */
    ssi_payload_t *payload = NULL;
    pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)uproc_pcb,
                                     (unsigned int)&payload, 0);
    switch (payload->service_code) {
    case GET_TOD:
      unsigned int tod;
      STCK(tod);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, tod, 0);
      break;
    case TERMINATE:
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      SYSCALL(SENDMESSAGE, (unsigned int)sup_init_pcb, 0, 0);

      ssi_payload_t term_process_payload = {
          .service_code = TERMPROCESS,
          .arg = (void *)NULL,
      };
      SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
              (unsigned int)(&term_process_payload), 0);
      SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);

      break;
    case WRITEPRINTER:
      dev_id.dev_class = PRNT_CLASS;
      dev_id.dev_number = sender->p_supportStruct->sup_asid - 1;
      print(dev_id, (sst_print_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    case WRITETERMINAL:
      dev_id.dev_class = TERM_CLASS;
      dev_id.dev_number = sender->p_supportStruct->sup_asid - 1;
      print(dev_id, (sst_print_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    default:
      PANIC();
      break;
    }
  }
}

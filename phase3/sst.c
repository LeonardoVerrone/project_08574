#include "headers/sst.h"

#include "../phase2/headers/devices.h"
#include "headers/common.h"
#include "headers/sysSupport.h"

#include <types.h>

#include <umps/cp0.h>
#include <umps/libumps.h>
#include <umps/types.h>

extern pcb_t *ssi_pcb;

static pcb_t *create_uproc();
static void print(int, int, sst_print_t *);

void sst_handler() {
  pcb_t *uproc_pcb = create_uproc();

  while (TRUE) {
    ssi_payload_t *payload = NULL;
    pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)uproc_pcb,
                                     (unsigned int)&payload, 0);
    int sender_asid = sender->p_supportStruct->sup_asid;
    switch (payload->service_code) {
    case GET_TOD:
      unsigned int tod;
      STCK(tod);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, tod, 0);
      break;
    case TERMINATE:
      programTrapHandler(sender->p_supportStruct->sup_asid);
      break;
    case WRITEPRINTER:
      print(PRNT_CLASS, sender_asid - 1, (sst_print_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    case WRITETERMINAL:
      print(TERM_CLASS, sender_asid - 1, (sst_print_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    default:
      PANIC();
      break;
    }
  }
}

/*
 * Funzione che crea l'U-PROC figlio dell'SST. Viene prima inizializzato lo
 * stato dell'U-PROC impostandone EntryHi.ASID, PC e SP. Successivamente vengono
 * usati lo stato appena creato e il support dell'SST padre per create l'UPROC.
 */
static pcb_t *create_uproc() {
  /*
   * Inizializzo lo stato partendo da quello dell'SST dove ho già impostato
   * l'EntryHI.ASID corretto
   */
  state_t uproc_state;
  STST(&uproc_state);
  uproc_state.reg_sp = USERSTACKTOP;
  uproc_state.pc_epc = uproc_state.reg_t9 = UPROCSTARTADDR;
  uproc_state.status =
      ALLOFF | STATUS_KUp | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;

  return create_process(&uproc_state, get_support_data());
}

/*
 * Funzione che esegue la stampa della stringa passata sul device specificato,
 * il device deve essere un terminale, o una stampante.
 */
static void print(int dev_class, int dev_number, sst_print_t *print_payload) {
  devreg_t *devreg = (devreg_t *)compute_devreg_addr(dev_class, dev_number);
  for (int i = 0; i < print_payload->length; i++) {
    unsigned int *commandAddr;
    unsigned int commandValue;

    switch (dev_class) {
    case TERM_CLASS:
      termreg_t *termreg = &devreg->term;
      commandAddr = &termreg->transm_command;
      commandValue = TRANSMITCHAR | (print_payload->string[i] << 8);
      break;
    case PRNT_CLASS:
      dtpreg_t *dtpreg = &devreg->dtp;
      dtpreg->data0 = print_payload->string[i];
      commandAddr = &dtpreg->command;
      commandValue = PRINTCHAR;
      break;
    default:
      PANIC();
    }
    do_io(commandAddr, commandValue);
  }
}

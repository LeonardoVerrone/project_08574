#include "headers/sysSupport.h"

#include <const.h>
#include <types.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern pcb_t *ssi_pcb;
extern pcb_t *current_process;
extern pcb_t *mutex_holder;

extern pcb_t *sst_pcbs[UPROCMAX];

extern support_t *get_support_data();
extern void terminate_process(pcb_t *);
extern void release_swap_mutex();

extern void bp();
extern void klog_print(char *str);
extern void klog_print_hex(unsigned int);
extern void klog_print_dec(int);

void programTrapHandler() {
  if (current_process == mutex_holder) {
    release_swap_mutex();
  }
  terminate_process(0);
}

void sup_SyscallHandler(support_t *excp_support) {
  state_t *excp_state = &excp_support->sup_exceptState[GENERALEXCEPT];
  pcb_t *dest = (pcb_t *)excp_state->reg_a1;
  const unsigned int payload = excp_state->reg_a2;

  switch (excp_state->reg_a0) {
  case SENDMSG:
    if (dest == PARENT) {
      dest = sst_pcbs[excp_support->sup_asid - 1];
    }
    SYSCALL(SENDMESSAGE, (unsigned int)dest, payload, 0);
    break;
  case RECEIVEMSG:
    SYSCALL(RECEIVEMESSAGE, (unsigned int)dest, payload, 0);
    break;
  default:
    programTrapHandler();
    return;
  }

  excp_state->pc_epc += WORDLEN;
  LDST(excp_state);
}

void sup_ExceptionHandler() {
  support_t *excp_support = get_support_data();
  const int cause_code =
      CAUSE_GET_EXCCODE(excp_support->sup_exceptState[GENERALEXCEPT].cause);

  if (cause_code == EXC_SYS) {
    sup_SyscallHandler(excp_support);
  } else {
    programTrapHandler();
  }
}

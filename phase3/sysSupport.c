#include "headers/sysSupport.h"

#include "headers/common.h"

#include <types.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern pcb_t *test_pcb;
extern pcb_t *mutex_holder;
extern pcb_t *sst_pcbs[UPROCMAX];

extern void invalidate_uproc_frames(int);

static void sup_SyscallHandler(support_t *);

/*
 * Funzione che gestisce le eccezioni passate al livello supporto. Esse possono
 * essere: SYSCALL del livello support, o Program Trap
 */
void sup_ExceptionHandler() {
  support_t *excp_support = get_support_data();

  const int cause_code =
      CAUSE_GET_EXCCODE(excp_support->sup_exceptState[GENERALEXCEPT].cause);

  if (cause_code == EXC_SYS) {
    sup_SyscallHandler(excp_support);
  } else {
    programTrapHandler(excp_support->sup_asid);
  }
}

/*
 * Funzione che termina l'UPROC corrente in caso di trap.
 */
void programTrapHandler(int asid) {
  /* Rilascio la mutua esclusione della Swap Pool (se necessario) */
  if (mutex_holder->p_supportStruct->sup_asid == asid) {
    release_swap_mutex();
  }

  /* Segno come inutilizzati i frame della Swap Pool che occupavo */
  invalidate_uproc_frames(asid);

  /* Avviso il processo di test che sto terminando */
  SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0);

  terminate_process(NULL);
}

/*
 * Funzione che gestisce le System Call di livello supporto, in particolare
 * vengono gestite le SYSCALL di tipo SENDMSG e RECEIVEMSG.
 */
static void sup_SyscallHandler(support_t *excp_support) {
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
    programTrapHandler(excp_support->sup_asid);
    return;
  }

  excp_state->pc_epc += WORDLEN;
  LDST(excp_state);
}

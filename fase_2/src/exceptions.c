#include "exceptions.h"
#include "const.h"
#include "listx.h"
#include "msg.h"
#include "pcb.h"
#include "types.h"
#include "util.h"

#include <umps/cp0.h>

extern pcb_t *current_process;
extern struct list_head ready_queue;
extern struct list_head pcbFree_h;

void syscallHandler() {
  memcpy(&(current_process->state), (state_t *)BIOSDATAPAGE, sizeof(state_t));

  const int id = saved_exc_state.reg_a0;
  const int is_kernel = (saved_exc_state.status >> STATUS_KUp_BIT) & 1;
  const int is_user = !is_kernel;

  if (id < 0 && is_user) {
    setCAUSE(EXC_RI);
    // TODO: chiamare trapHandler
    return;
  }

  switch (id) {
  case SENDMESSAGE:
    const int dest_pid = saved_exc_state.reg_a1;
    pcb_t *dest;

    if (list_contains_pid(&pcbFree_h, dest_pid)) {
      // saved_exc_state.reg_v0 = DEST_NOT_EXIST;
      return;
    }
    if ((dest = list_search_by_pid(&ready_queue, dest_pid)) != NULL) {
      msg_t *msg = allocMsg();
      if (msg == NULL) {
        // saved_exc_state.reg_v0 = MSGNOGOOD;
        return;
      }
      msg->m_sender = current_process->p_pid;
      msg->m_payload = saved_exc_state.reg_a2;
      insertMessage(&(dest->msg_inbox), msg);
      return;
    }
    // TODO: if the process is waiting wake it up
    break;
  case RECEIVEMESSAGE:
    break;
  default:
  }
}

void exceptionHandler() {
  switch (getCAUSE()) {
  case EXC_INT:
    // TODO: interrupts handler
    break;
  case EXC_MOD:
  case EXC_TLBL:
  case EXC_TLBS:
    // TODO: tlb handler
    break;
  case 8:
    syscallHandler();
    break;
  default:
    // TODO: trap handler
    break;
  }
}

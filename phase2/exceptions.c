#include "headers/exceptions.h"

#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/globals.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"
#include "headers/util.h"

#include <const.h>
#include <listx.h>
#include <types.h>

#include <umps/cp0.h>
#include <umps/libumps.h>

static void passUpOrDie(int index) {
  if (current_process == NULL) {
    schedule(NULL);
    return;
  }

  if (current_process->p_supportStruct == NULL) {
    kill_process(current_process);
    schedule(NULL);
    return;
  }

  memcpy(&current_process->p_supportStruct->sup_exceptState[index],
         &current_process->p_s, sizeof(state_t));

  struct context_t context =
      current_process->p_supportStruct->sup_exceptContext[index];

  LDCXT(context.stackPtr, context.status, context.pc);
}

void syscallHandler() {
  state_t *stored_state = &current_process->p_s;

  const int cause = stored_state->reg_a0;

  if (cause >= 1) {
    passUpOrDie(GENERALEXCEPT);
    return;
  }

  // se sono in modalita USER, allora passo genero una trap
  const int is_user = (stored_state->status >> STATUS_KUp_BIT) & 1;
  if (is_user) {
    setCAUSE(EXC_RI);
    passUpOrDie(GENERALEXCEPT);
    return;
  }

  if (cause == SENDMESSAGE) {
    // pcb destinatario
    pcb_t *dest = (pcb_t *)stored_state->reg_a1;

    // se il pcb destinatario non è allocato restituisco DEST_NOT_EXIST
    if (contains_pcb(&pcbFree_h, dest)) {
      stored_state->reg_v0 = DEST_NOT_EXIST;
      stored_state->pc_epc += WORDLEN;
      schedule(current_process);
      return;
    }

    // se il destinatario sta aspettando un messaggio lo sblocco
    if (contains_pcb(&waiting_for_msg, dest)) {
      soft_block_count--;
      outProcQ(&(waiting_for_msg), dest);
      insertProcQ(&(ready_queue), dest);
    }

    // creo ed invio il messaggio
    msg_t *msg = allocMsg();
    if (msg == NULL) {
      stored_state->reg_v0 = MSGNOGOOD;
    } else {
      stored_state->reg_v0 = 0; // codice di ritorno
      msg->m_sender = current_process;
      msg->m_payload = stored_state->reg_a2;

      insertMessage(&(dest->msg_inbox), msg);
    }

    stored_state->pc_epc += WORDLEN;
    schedule(current_process);
  }

  if (cause == RECEIVEMESSAGE) {
    pcb_t *sender = (pcb_t *)stored_state->reg_a1;
    if (sender == ANYMESSAGE)
      sender = NULL;

    // controllo se ho messaggi nell'inbox, in caso negativo mi blocco
    msg_t *msg = popMessage(&current_process->msg_inbox, sender);
    if (msg == NULL) {
      soft_block_count++;
      insertProcQ(&waiting_for_msg, current_process);
      schedule(NULL);
    } else {
      stored_state->reg_v0 = (unsigned int)msg->m_sender; // codice di ritorno

      // salvo il payload del messaggio
      unsigned int *a2 = (unsigned int *)stored_state->reg_a2;
      if (a2 != NULL) {
        *a2 = msg->m_payload;
      }
      freeMsg(msg);

      stored_state->pc_epc += WORDLEN;
      schedule(current_process);
    }
  }
}

void exceptionHandler() {
  // aggiorno il cpu_time di current_procesi current_processs, e copio lo stato
  // al momento della chiamata
  if (current_process) {
    updateCpuTime(current_process);
    memcpy(&current_process->p_s, (state_t *)BIOSDATAPAGE, sizeof(state_t));
  }

  switch (CAUSE_GET_EXCCODE(getCAUSE())) {
  case EXC_INT:
    interruptHandler();
    break;
  case EXC_MOD:
  case EXC_TLBL:
  case EXC_TLBS:
    passUpOrDie(PGFAULTEXCEPT);
    break;
  case 8:
    syscallHandler();
    break;
  default:
    passUpOrDie(GENERALEXCEPT);
    break;
  }
}

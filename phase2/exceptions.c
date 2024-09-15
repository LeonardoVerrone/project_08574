#include "headers/exceptions.h"

#include "../phase1/headers/msg.h"
#include "../phase1/headers/pcb.h"
#include "headers/globals.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"
#include "headers/util.h"

#include <const.h>
#include <listx.h>
#include <types.h>

#include <umps/cp0.h>
#include <umps/libumps.h>

static void passUpOrDie(int);
static void syscallHandler();

/*
 * Funzione che implementa il gestore delle eccezioni, in particolare è la
 * funzione a cui viene passato il controlo ogni volta che si verifica
 * un'eccezione. Viene controllata la causa dell'eccezione, nel caso in cui si
 * tratti di un system call o di un interrupt, viene passato il controllo ai
 * relativi gestore, altrimenti viene eseguita la procedura di Pass Up or Die.
 *
 * NOTA: da qui in poi il processore si trova in modalità kernel con gli
 * interrupt disabilitati. Questa condizione cessa quando faccio un nuovo
 * schedule, o quando passo il controllo al livello supporto.
 */
void exceptionHandler() {
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
  case EXC_SYS:
    syscallHandler();
    break;
  default:
    passUpOrDie(GENERALEXCEPT);
    break;
  }
}

/*
 * Funzione che dato imlementa il comportamento "passUpOrDie".
 * Controlla se il processo ha specificata la Struttura di Supporto, in caso
 * positivo passa la gestione dell'eccezione al livello Supporto, altrimenti
 * termina il processo stesso.
 */
static void passUpOrDie(int excp_index) {
  if (current_process == NULL) {
    schedule(NULL);
    return;
  }

  if (current_process->p_supportStruct == NULL) {
    kill_process(current_process);
    schedule(NULL);
    return;
  }

  memcpy(&current_process->p_supportStruct->sup_exceptState[excp_index],
         &current_process->p_s, sizeof(state_t));

  struct context_t context =
      current_process->p_supportStruct->sup_exceptContext[excp_index];

  LDCXT(context.stackPtr, context.status, context.pc);
}

/*
 * Funzione che implementa il gestore delle System Call SYS1 e SYS2. Viene
 * controllato il codice della SYSCALL, se esso è positivo allora viene fatto il
 * passUpOrDie. Lo stesso avviene se il processo ha scatenato una System Call
 * SYS1, o SYS2, mentre è in modalità 'user'.
 */
static void syscallHandler() {
  state_t *excp_state = &current_process->p_s;

  const int cause = excp_state->reg_a0;

  /* Pass up Support Level's exceptions. */
  if (cause >= 1) {
    passUpOrDie(GENERALEXCEPT);
    return;
  }

  /*  if is user then passUpOrDie */
  {
    const int is_user = (excp_state->status >> STATUS_KUp_BIT) & 1;
    if (is_user) {
      setCAUSE(EXC_RI);
      passUpOrDie(GENERALEXCEPT);
      return;
    }
  }

  /*
   * SYS1 (SENDMESSAGE)
   * Recapita il messagio al destinatario: se esso è in attesa viene messo in
   * stato READY READY, successivamente viene inserito il messaggio nell'inbox
   * del destinatario
   */
  if (cause == SENDMESSAGE) {
    // pcb destinatario
    pcb_t *dest = (pcb_t *)excp_state->reg_a1;

    // se il pcb destinatario non è allocato restituisco DEST_NOT_EXIST
    if (contains_pcb(&pcbFree_h, dest)) {
      excp_state->reg_v0 = DEST_NOT_EXIST;
      excp_state->pc_epc += WORDLEN;
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
      excp_state->reg_v0 = MSGNOGOOD;
    } else {
      excp_state->reg_v0 = 0; // codice di ritorno
      msg->m_sender = current_process;
      msg->m_payload = excp_state->reg_a2;

      insertMessage(&(dest->msg_inbox), msg);
    }

    excp_state->pc_epc += WORDLEN;
    schedule(current_process);
  }

  /*
   * SYS2 - RECEIVEMESSAGE
   * Ricevo un messaggio da un certo destinatario, (o da tutti se
   * dest==ANYMESSAGE): se il messaggio è già presente nell'inbox viene
   * restituito il payload dello stesso,altrimenti il processo viene messo in
   * attesa.
   */
  if (cause == RECEIVEMESSAGE) {
    pcb_t *sender = (pcb_t *)excp_state->reg_a1;
    if (sender == ANYMESSAGE)
      sender = NULL;

    // controllo se ho messaggi nell'inbox, in caso negativo mi blocco
    msg_t *msg = popMessage(&current_process->msg_inbox, sender);
    if (msg == NULL) {
      soft_block_count++;
      insertProcQ(&waiting_for_msg, current_process);
      schedule(NULL);
    }

    excp_state->reg_v0 = (unsigned int)msg->m_sender; // codice di ritorno

    // salvo il payload del messaggio
    unsigned int *a2 = (unsigned int *)excp_state->reg_a2;
    if (a2 != NULL) {
      *a2 = msg->m_payload;
    }
    freeMsg(msg);

    excp_state->pc_epc += WORDLEN;
    schedule(current_process);
  }
}

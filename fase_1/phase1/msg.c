#include "./headers/msg.h"

static msg_t msgTable[MAXMESSAGES];
LIST_HEAD(msgFree_h);

void initMsgs() {
  INIT_LIST_HEAD(&msgFree_h);

  for (int i = 0; i < MAXMESSAGES; i++) {
    // aggiungo a msgFree m_list di ogni msg
    list_add(&msgTable[i].m_list, &msgFree_h);
  }
}

void freeMsg(msg_t *m) {
  if (m == NULL)
    return;

  // devo rimuoverlo dalla coda dei msg del pcb a cui appartiene(pcb.msg_inbox)
  list_del(&m->m_list);
  // e aggiungerlo ai msg liberi
  list_add(&m->m_list, &msgFree_h);
}

msg_t *allocMsg() {
  if (list_empty(&msgFree_h))
    return NULL;

  msg_t *m = container_of(msgFree_h.next, msg_t, m_list);
  list_del(msgFree_h.next);

  // inizializzo
  INIT_LIST_HEAD(&m->m_list);
  m->m_sender = NULL;
  m->m_payload = 0;

  return m;
}

void mkEmptyMessageQ(struct list_head *head) {
  if (head == NULL) {
    return;
  }

  INIT_LIST_HEAD(head);
}

int emptyMessageQ(struct list_head *head) {
  if (head == NULL)
    return 1;

  return list_empty(head);
}

void insertMessage(struct list_head *head, msg_t *m) {
  if (head == NULL || m == NULL)
    return;

  list_add_tail(&m->m_list, head);
}

void pushMessage(struct list_head *head, msg_t *m) {
  if (head == NULL || m == NULL)
    return;

  list_add(&m->m_list, head); // aggiunge in testa
}

/*
 * Leo: ho sistemato questo metodo perché da consegna deve restituire il primo
 * messaggio che abbia come parent il pcb specificato.
 */
msg_t *popMessage(struct list_head *head, pcb_t *p_ptr) {
  /*
   * Leo: ho rimosso il controllo se il processo parent ha messaggi in inbox
   * perché falliva un test. Se vedi dentro al file p1test.c a riga 312 lui
   * prepara il messaggio e il pcb parent, ma a quest'ultimo non metto il
   * messaggio in inbox
   */
  // if (head == NULL || list_empty(head) || list_empty(&p_ptr->msg_inbox))
  if (head == NULL || list_empty(head))
    return NULL;

  // Leo: da requisiti se il parent è NULL restituisco il primo messaggio della
  // coda
  if (p_ptr == NULL){
    struct list_head *tmp = head->next; //ecco perché dava errore
    list_del(head->next); //Luca: va rimosso
    return container_of(tmp, msg_t, m_list);
  }

  // Leo: faccio la ricerca del messaggio che ha come sender il pcb passato
  struct list_head *iter;
  int found = 0;
  list_for_each(iter, head) { //cicla sulla lista di head, aggiornando iter
    msg_t *m = container_of(iter, msg_t, m_list);
    if (m->m_sender == p_ptr) {
      found = 1;
      break;
    }
  }

  // Leo: da requisiti, se non trovo nessun messaggio con sender = p_ptr allora
  // restituisco NULL
  if (!found)
    return NULL;

  msg_t *result = container_of(iter, msg_t, m_list);
  list_del(iter);
  return result;
}

msg_t *headMessage(struct list_head *head) {
  if (head == NULL || list_empty(head)) {
    return NULL;
  }

  return container_of(head->next, msg_t, m_list);
}

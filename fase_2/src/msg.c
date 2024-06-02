#include "msg.h"

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

  list_del(&m->m_list);
  list_add(&m->m_list, &msgFree_h);
}

msg_t *allocMsg() {
  if (list_empty(&msgFree_h))
    return NULL;

  // recupero msg in cima alla lista dei msg liberi e lo rimuovo dalla lista
  msg_t *m = container_of(msgFree_h.next, msg_t, m_list);
  list_del(msgFree_h.next);

  // inizializzo msg
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

int emptyMessageQ(struct list_head *head) { // ritorna 1 se la lista è vuota
  if (head == NULL)
    return 1;

  return list_empty(head);
}

void insertMessage(struct list_head *head, msg_t *m) { // in coda
  if (head == NULL || m == NULL)
    return;

  list_add_tail(&m->m_list, head);
}

void pushMessage(struct list_head *head, msg_t *m) { // aggiunge in testa
  if (head == NULL || m == NULL)
    return;

  list_add(&m->m_list, head);
}

msg_t *popMessage(struct list_head *head, pcb_t *p_ptr) {
  if (head == NULL || list_empty(head))
    return NULL;

  if (p_ptr == NULL) {
    // faccio pop dell'head
    struct list_head *tmp = head->next;
    list_del(head->next);
    return container_of(tmp, msg_t, m_list);
  }

  // cerco messaggio che ha p_ptr come sender
  struct list_head *iter;
  int found = 0;
  list_for_each(iter, head) {
    msg_t *m = container_of(iter, msg_t, m_list);
    if (m->m_sender == p_ptr) {
      found = 1;
      break;
    }
  }

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

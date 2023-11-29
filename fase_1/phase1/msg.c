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
  if(m == NULL)
    return;
  
  // devo rimuoverlo dalla coda dei msg del pcb a cui appartiene(pcb.msg_inbox)
  list_del(&m->m_list);
  // e aggiungerlo ai msg liberi
  list_add(&m->m_list, &msgFree_h);
}

msg_t *allocMsg() {
  if( list_empty(&msgFree_h) )
    return NULL;
  
  msg_t *m = container_of(msgFree_h.next, msg_t, m_list);
  list_del(msgFree_h.next);
  
  //inizializzo
  INIT_LIST_HEAD(&m->m_list);
  m->m_sender = NULL;
  m->m_payload = 0;
  
  return m;
}

void mkEmptyMessageQ(struct list_head *head) {
}

int emptyMessageQ(struct list_head *head) {
}

void insertMessage(struct list_head *head, msg_t *m) {
}

void pushMessage(struct list_head *head, msg_t *m) {
}

msg_t *popMessage(struct list_head *head, pcb_t *p_ptr) {
}

msg_t *headMessage(struct list_head *head) {
}

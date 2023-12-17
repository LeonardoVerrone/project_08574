#include "./headers/pcb.h"
// #include <stdlib.h> tanto free da errore, quindi non mi serve

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

static struct list_head *list_search(const struct list_head *head, const pcb_t *p){
  // funzione di supporto per trovare il puntatore alla list_head di un pcb
  struct list_head *iter;
  list_for_each(iter, head) {
    if (iter == &p->p_list) {
      return iter;
    }
  }
  return NULL;
}

static inline int list_contains(const struct list_head *head, const pcb_t *p) {
  return list_search(head, p) != NULL;
}

void initPcbs() {
  // inizializzo pcbFree_h
  INIT_LIST_HEAD(&pcbFree_h);

  for (int i = 0; i < MAXPROC; i++) {
    // aggiungo a pcbFree il p_list di ogni pcb
    list_add(&pcbTable[i].p_list, &pcbFree_h);
  }
}

void freePcb(pcb_t *p) {
  if (p == NULL || list_contains(&pcbFree_h, p))
    return;

  // devo inserire p in pcbFree_h
  // 1. lo elimino dalla coda dei processi
  list_del(&p->p_list);

  // 2. lo aggiungo a pcbFree
  list_add(&p->p_list, &pcbFree_h);
}

void initState(state_t *p_s){
  // per inizializzare una variabile state_t (in fase 2 potrebbero essere necessarie modifiche)
  p_s->entry_hi = 0;
  p_s->cause = 0;
  p_s->status = 0;
  p_s->pc_epc = 0;
  for(int i=0; i<STATE_GPR_LEN; i++){
    p_s->gpr[i] = 0;
  }
  p_s->hi = 0;
  p_s->lo = 0;
}

pcb_t *allocPcb() {
  if (list_empty(&pcbFree_h)) // se la lista è vuota
    return NULL;

  // prendo il primo dalla lista dei pcb liberi
  pcb_t *p = container_of(pcbFree_h.next, pcb_t, p_list);
  list_del(pcbFree_h.next);

  // init process queue
  INIT_LIST_HEAD(&p->p_list);

  // init process tree fields
  p->p_parent = NULL;
  INIT_LIST_HEAD(&p->p_child);
  INIT_LIST_HEAD(&p->p_sib);

  // init process status information
  // processor state è un tipo di umps3, si trova sulla pagina github di umps3 in umps3/src/support/libumps/tipes.h
  initState(&p->p_s); // per p->p_s
  p->p_time = 0;

  // init message queue
  INIT_LIST_HEAD(&p->msg_inbox);

  // init pointer to the support struct
  // free(p->p_supportStruct); DA' ERRORE UNDEFINED REFERENCE TO FREE -> rischio MEMORY LEAK?
  p->p_supportStruct = NULL; // da specifiche fase 2 sembra dover andare a NULL, ma non rischio memory leak senza free?
  
  // init pid
  p->p_pid = 0;

  return p;
}

void mkEmptyProcQ(struct list_head *head) {
  if (head == NULL) {
    return;
  }

  INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head *head) { // ritorna 1 se lista è vuota
  if (head == NULL) {
    return 1;
  }

  return list_empty(head);
}

void insertProcQ(struct list_head *head, pcb_t *p) { // in coda
  if (head == NULL || p == NULL) {
    return;
  }

  list_add_tail(&p->p_list, head);
}

pcb_t *headProcQ(struct list_head *head) { // ritorna primo pcb
  if (head == NULL || list_empty(head)) {
    return NULL;
  }

  return container_of(head->next, pcb_t, p_list);
}

pcb_t *removeProcQ(struct list_head *head) { // in testa
  if (head == NULL || list_empty(head)) {
    return NULL;
  }

  pcb_t *p = container_of(head->next, pcb_t, p_list);
  list_del(head->next);
  return p;
}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) { // rimuove p
  if (head == NULL || p == NULL || list_empty(head) || !list_contains(head, p)) {
    return NULL;
  }

  list_del(&p->p_list);

  return p;
}

// lista dei figli:
//  1) p_child del padre funge da elemento sentinella
//  2) i figli sono collegati fra loro dai puntatori p_sib
int emptyChild(pcb_t *p) {
  if (p == NULL) // <- serve davvero o abbiamo garanzia p != NULL?
    return 1;

  return list_empty(&p->p_child); // p.p_child è la testa della lista
}

void insertChild(pcb_t *prnt, pcb_t *p) { // in coda
  if (prnt == NULL || p == NULL || p->p_parent != NULL ||
      list_contains(&prnt->p_child, p)) // ?
    return;

  list_add_tail(&p->p_sib, &prnt->p_child);
  p->p_parent = prnt;
}

pcb_t *removeChild(pcb_t *p) { // disereda il primo figlio
  if (p == NULL || emptyChild(p))
    return NULL;

  return outChild(container_of(p->p_child.next, pcb_t, p_sib));
  // in pratica passo il primo figlio a una funzione che lo rimuove dalla lista del padre
}

pcb_t *outChild(pcb_t *p) { // p non è più figlio di suo padre
  if (p == NULL || p->p_parent == NULL || emptyChild(p->p_parent))
    return NULL;

  list_del(&p->p_sib);
  p->p_parent = NULL;
  INIT_LIST_HEAD(&p->p_sib);
  return p;
}

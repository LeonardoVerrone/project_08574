#include "./headers/pcb.h"
#include "../klog.c"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

static struct list_head *list_search(const struct list_head *head,
                                     const pcb_t *p) {
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
  // 1. elimino la coda dei processi
  list_del(&p->p_list);

  // 2. lo aggiungo a pcbFree
  // NOTA: non serve aggiungere in fondo perché pcbFree non è una coda
  list_add(&p->p_list, &pcbFree_h);
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
  // TODO: processor state (non ho trovato neanche la dichiarazione di state_t)
  p->p_time = 0;

  // init message queue
  INIT_LIST_HEAD(&p->msg_inbox);

  // init pointer to the support struct
  // TODO: non ho capito cosa sia

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

int emptyProcQ(struct list_head *head) {
  if (head == NULL) {
    return 1;
  }

  return list_empty(head);
}

void insertProcQ(struct list_head *head, pcb_t *p) {
  if (head == NULL || p == NULL) {
    return;
  }

  list_add_tail(&p->p_list, head);
}

pcb_t *headProcQ(struct list_head *head) {
  if (head == NULL || list_empty(head)) {
    return NULL;
  }

  return container_of(head->next, pcb_t, p_list);
}

pcb_t *removeProcQ(struct list_head *head) {
  if (head == NULL || list_empty(head)) {
    return NULL;
  }

  pcb_t *p = container_of(head->next, pcb_t, p_list);
  list_del(head->next);
  return p;
}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
  if (head == NULL || p == NULL || list_empty(head) ||
      !list_contains(head, p)) {
    return NULL;
  }

  list_del(&p->p_list);

  return p;
}

/*
 * Leo: per quanto riguardo le guardie all'inizio dei metodi: siccome questi
 * metodi li possiamo considerare "di libreria", ovvero che verranno usati da
 * tutto il S.O. per la gestione dei processi, dobbiamo assicurarci l'integrità
 * dei parametri passati
 */

// lista dei figli:
//  1) p_child del padre funge da elemento sentinella
//  2) i figli sono collegati fra loro dai puntatori p_sib
int emptyChild(pcb_t *p) {
  if (p == NULL) // <- serve davvero o abbiamo garanzia p != NULL?
    return 1;

  return list_empty(&p->p_child); // p.p_child è la testa della lista
}

void insertChild(pcb_t *prnt, pcb_t *p) {
  /*
   * Leo: ho aggiunto guardie per verificare che *p non abbia già un parent e
   * che NON sia già figlio di *prnt
   */
  if (prnt == NULL || p == NULL || p->p_parent != NULL ||
      list_contains(&prnt->p_child, p)) // ?
    return;

  list_add_tail(&p->p_sib, &prnt->p_child);
  p->p_parent = prnt;
}

/*
 * Leo: Se noti quello che viene fatto su *sib, è lo stesso che farei chiamando
 * direttamente il metodo outChild
 */
pcb_t *removeChild(pcb_t *p) {
  if (p == NULL || emptyChild(p)) //?
    return NULL;

  return outChild(container_of(p->p_child.next, pcb_t, p_sib));

  /*
  if(p == NULL) //?
    return NULL;

  if( !emptyChild(p) ){
    struct list_head *sib = p->p_child.next; // = punta p_sib del primo figlio
    list_del(sib);
    INIT_LIST_HEAD(sib); //reinizializzo p_sib del figlio staccato
    return container_of( sib, pcb_t, p_sib); // (puntatore al list_head della
  strutt, tipo strutt, nome list_head nella strutt)
  }
  else
    return NULL;
  */
}

/*
 * Leo: ho sistemato le guardie, se noti il controllo 'p->p_parent != NULL' può
 * essere portato in cima
 */
pcb_t *outChild(pcb_t *p) {
  if (p == NULL || p->p_parent == NULL || emptyChild(p->p_parent))
    return NULL;

  list_del(&p->p_sib);
  p->p_parent = NULL;
  INIT_LIST_HEAD(&p->p_sib);
  return p;

  /*
  if(p == NULL) //?
    return NULL;

  if( p->p_parent != NULL){
    list_del(&p->p_sib);
    p->p_parent = NULL;
    INIT_LIST_HEAD(&p->p_sib);
    return p;
  }
  else
    return NULL;
  */
}

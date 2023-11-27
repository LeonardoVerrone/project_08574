#include "./headers/pcb.h"
#include "../klog.c"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

void initPcbs() {
  // inizializzo pcbFree_h
  INIT_LIST_HEAD(&pcbFree_h);

  for (int i = 0; i < MAXPROC; i++) {
    // aggiungo a pcbFree il p_list di ogni pcb
    list_add(&pcbTable[i].p_list, &pcbFree_h);
  }
}

void freePcb(pcb_t *p) {
  // TODO: aggiungere controllo se p è già contenuto in pcbFree_h
  if (p == NULL)
    return;

  // devo inserire p in pcbFree_h
  // 1. elimino la coda dei processi
  list_del(&p->p_list);

  // 2. lo aggiungo a pcbFree
  // NOTA: non serve aggiungere in fondo perché pcbFree non è una coda
  list_add(&p->p_list, &pcbFree_h);

  // struct list_head *list_address = &(p->p_list);
  // list_add_tail(list_address,&pcbFree_h); //aggiunto in fondo
}

pcb_t *allocPcb() {
  if (list_empty(&pcbFree_h)) // se la lista è vuota
    return NULL;

  // prendo il primo dalla lista dei pcb liberi
  pcb_t *p = container_of(pcbFree_h.next, pcb_t, p_list);
  list_del(pcbFree_h.next);

  // init process queue
  p->p_list.prev = NULL;
  p->p_list.next = NULL;

  // init process tree fields
  p->p_parent = NULL;
  p->p_child.prev = NULL;
  p->p_child.next = NULL;
  p->p_sib.prev = NULL;
  p->p_sib.next = NULL;

  // init process status information
  // TODO: processor state (non ho trovato neanche la dichiarazione di state_t)
  p->p_time = 0;

  // init message queue
  p->msg_inbox.prev = NULL;
  p->msg_inbox.next = NULL;

  // init pointer to the support struct
  // TODO: non ho capito cosa sia

  // init pid
  p->p_pid = 0;

  return p;

  // struct list_head *p = pcbFree_h.next;
  // list_del(p); //cancella p dalla sua lista
  // //reinizializzo il pcb che contiene il puntatore p
  // struct pcb_t *pcb_libero = container_of(p,pcb_t,p_list); //Macro che
  // restituisce il puntatore all'istanza della struttura che contiene un certo
  // list_head
  // /* dà errori di tipi, da guardare
  // LIST_HEAD_INIT(pcb_libero->p_list);
  // pcb_libero->p_parent = NULL;
  // LIST_HEAD_INIT(pcb_libero->p_child);
  // LIST_HEAD_INIT(pcb_libero->p_sib);
  // //pcb_libero->p_s = 0; //stato proc = ???
  // pcb_libero->p_time = 0;
  // LIST_HEAD_INIT(pcb_libero->msg_inbox);
  // pcb_libero->p_supportstruct = NULL;
  // */
}

void mkEmptyProcQ(struct list_head *head) {}

int emptyProcQ(struct list_head *head) {}

void insertProcQ(struct list_head *head, pcb_t *p) {}

pcb_t *headProcQ(struct list_head *head) {}

pcb_t *removeProcQ(struct list_head *head) {}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {}

int emptyChild(pcb_t *p) {}

void insertChild(pcb_t *prnt, pcb_t *p) {}

pcb_t *removeChild(pcb_t *p) {}

pcb_t *outChild(pcb_t *p) {}

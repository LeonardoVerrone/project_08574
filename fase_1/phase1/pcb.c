#include "./headers/pcb.h"
#include "../klog.c"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

void initPcbs() {
      klog_print("here!");
}

void freePcb(pcb_t *p) {
  struct list_head *list_address = &(p->p_list);
  list_add_tail(list_address,&pcbFree_h); //aggiunto in fondo
}

pcb_t *allocPcb() {
  if(list_empty(&pcbFree_h) == 1) //se la lista è vuota
    return NULL;
  struct list_head *p = pcbFree_h.next;
  list_del(p); //cancella p dalla sua lista
  //reinizializzo il pcb che contiene il puntatore p
  struct pcb_t *pcb_libero = container_of(p,pcb_t,p_list); //Macro che restituisce il puntatore all'istanza della struttura che contiene un certo list_head
  /* dà errori di tipi, da guardare
  LIST_HEAD_INIT(pcb_libero->p_list);
  pcb_libero->p_parent = NULL;
  LIST_HEAD_INIT(pcb_libero->p_child);
  LIST_HEAD_INIT(pcb_libero->p_sib);
  //pcb_libero->p_s = 0; //stato proc = ???
  pcb_libero->p_time = 0;
  LIST_HEAD_INIT(pcb_libero->msg_inbox);
  pcb_libero->p_supportstruct = NULL;
  */
}

void mkEmptyProcQ(struct list_head *head) {
}

int emptyProcQ(struct list_head *head) {
}

void insertProcQ(struct list_head *head, pcb_t *p) {
}

pcb_t *headProcQ(struct list_head *head) {
}

pcb_t *removeProcQ(struct list_head *head) {
}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
}

int emptyChild(pcb_t *p) {
}

void insertChild(pcb_t *prnt, pcb_t *p) {
}

pcb_t *removeChild(pcb_t *p) {
}

pcb_t *outChild(pcb_t *p) {
}

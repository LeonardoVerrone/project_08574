#ifndef PCB_H_INCLUDED
#define PCB_H_INCLUDED

#include "types.h"
#include "listx.h"

struct list_head *list_search(const struct list_head *head, const pcb_t* p);
int list_contains(const struct list_head *head, const pcb_t* p);

struct list_head *list_search_by_pid(const struct list_head *head, const int p_pid);
int list_contains_pid(const struct list_head *head, const int p_pid);

void initPcbs();
void freePcb(pcb_t *p);
pcb_t *allocPcb();
void mkEmptyProcQ(struct list_head *head);
int emptyProcQ(struct list_head *head);
void insertProcQ(struct list_head *head, pcb_t *p);
pcb_t *headProcQ(struct list_head *head);
pcb_t *removeProcQ(struct list_head *head);
pcb_t *outProcQ(struct list_head *head, pcb_t *p);
int emptyChild(pcb_t *p);
void insertChild(pcb_t *prnt, pcb_t *p);
pcb_t *removeChild(pcb_t *p);
pcb_t *outChild(pcb_t *p);


#endif

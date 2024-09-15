#include "headers/globals.h"

#include "../phase1/headers/pcb.h"

int process_count;
int soft_block_count;

pcb_t *ssi_pcb;
pcb_t *current_process;

struct list_head ready_queue;
struct list_head waiting_for_PC;
struct list_head waiting_for_msg;

pcb_t *waiting_for_IO[NUMBER_OF_DEVICES];

/*
 * Funzione che inizializza le strutture di supporto al Nucleus
 */
void init_globals() {
  process_count = 0;
  soft_block_count = 0;

  ssi_pcb = NULL;
  current_process = NULL;

  mkEmptyProcQ(&ready_queue);
  mkEmptyProcQ(&waiting_for_PC);
  mkEmptyProcQ(&waiting_for_msg);

  for (int i = 0; i < NUMBER_OF_DEVICES; i++)
    waiting_for_IO[i] = NULL;
}

#include "util.h"

inline void state_memcpy(state_t *dest, const state_t *src) {
  dest->entry_hi = src->entry_hi;
  dest->cause = src->cause;
  dest->status = src->status;
  dest->pc_epc = src->pc_epc;
  dest->hi = src->hi;
  dest->lo = src->lo;
  // for (int i = 0; i < STATE_GPR_LEN; i++) {
  //   dest->gpr[i] = src->gpr[i];
  // }
  dest->gpr[0] = src->gpr[0];
  dest->gpr[1] = src->gpr[1];
  dest->gpr[2] = src->gpr[2];
  dest->gpr[3] = src->gpr[3];
  dest->gpr[4] = src->gpr[4];
  dest->gpr[5] = src->gpr[5];
  dest->gpr[6] = src->gpr[6];
  dest->gpr[7] = src->gpr[7];
  dest->gpr[8] = src->gpr[8];
  dest->gpr[9] = src->gpr[9];
  dest->gpr[10] = src->gpr[10];
  dest->gpr[11] = src->gpr[11];
  dest->gpr[12] = src->gpr[12];
  dest->gpr[13] = src->gpr[13];
  dest->gpr[14] = src->gpr[14];
  dest->gpr[15] = src->gpr[15];
  dest->gpr[16] = src->gpr[16];
  dest->gpr[17] = src->gpr[17];
  dest->gpr[18] = src->gpr[18];
  dest->gpr[19] = src->gpr[19];
  dest->gpr[20] = src->gpr[20];
  dest->gpr[21] = src->gpr[21];
  dest->gpr[22] = src->gpr[22];
  dest->gpr[23] = src->gpr[23];
  dest->gpr[24] = src->gpr[24];
  dest->gpr[25] = src->gpr[25];
  dest->gpr[26] = src->gpr[26];
  dest->gpr[27] = src->gpr[27];
  dest->gpr[28] = src->gpr[28];
}

void context_memcpy(context_t *dest, const context_t *src) {
  dest->stackPtr = src->stackPtr;
  dest->status = src->status;
  dest->pc = src->pc;
}

inline int contains_pcb(const struct list_head *head, const pcb_t *pcb) {
  pcb_t *p;
  int i = 0;
  list_for_each_entry(p, head, p_list) {
    if (p == pcb)
      return 1;
    if (++i > 10000) {
      bp();
      return 0;
    }
  }
  return 0;
}

inline void bp() {}

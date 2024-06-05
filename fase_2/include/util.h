#ifndef UTIL_HEADER
#define UTIL_HEADER

#include "types.h"

#define size_t unsigned long

void state_memcpy(state_t *dest, const state_t *src);
void support_memcpy(support_t *dest, const support_t *src);
void context_memcpy(context_t *dest, const context_t *src);

int contains_pcb(const struct list_head *head, const pcb_t *p);

void bp();

#endif // !UTIL_HEADER

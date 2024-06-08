#ifndef EXCEPTIONS_HEADER
#define EXCEPTIONS_HEADER
#include "types.h"

#define DEST_NOT_EXIST -2

void exceptionHandler();
void syscallHandler();
void initExceptionHandler();
void kill_process(pcb_t *p);

#endif // !EXCEPTIONS_HEADER

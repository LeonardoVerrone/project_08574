#include "headers/common.h"

#include "headers/vmSupport.h"

#include <umps/cp0.h>
#include <umps/libumps.h>

extern pcb_t *ssi_pcb;
extern pcb_t *test_pcb;
extern pcb_t *sw_mutex_pcb;

pcb_t *create_process(state_t *s, support_t *t) {
  pcb_t *p;
  ssi_create_process_t ssi_create_process = {
      .state = s,
      .support = t,
  };
  ssi_payload_t payload = {
      .service_code = CREATEPROCESS,
      .arg = &ssi_create_process,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&p), 0);
  return p;
}

support_t *get_support_data() {
  support_t *s;
  ssi_payload_t getsup_payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsup_payload),
          0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&s, 0);
  return s;
}

void terminate_process(pcb_t *proc) {
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)proc,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

unsigned int do_io(unsigned int *commandAddr, unsigned int commandValue) {
  unsigned int status;
  ssi_do_io_t do_io = {.commandAddr = commandAddr,
                       .commandValue = commandValue};
  ssi_payload_t payload = {.service_code = DOIO, .arg = &do_io};

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&status, 0);

  return status;
}

int get_missing_page(unsigned int entry_hi) {
  int missing_page = ENTRYHI_GET_VPN(entry_hi);
  if (missing_page > MAXPAGES) { // TRUE iff the missing page is the stack page
    missing_page = MAXPAGES - 1;
  }
  return missing_page;
}

void acquire_swap_mutex() {
  SYSCALL(SENDMESSAGE, (unsigned int)sw_mutex_pcb, 0, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)sw_mutex_pcb, 0, 0);
}

void release_swap_mutex() {
  SYSCALL(SENDMESSAGE, (unsigned int)sw_mutex_pcb, 0, 0);
}

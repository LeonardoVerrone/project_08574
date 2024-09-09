#include "headers/initProc.h"
#include "../phase2/headers/globals.h"
#include "headers/sst.h"
#include "headers/sysSupport.h"
#include "headers/vmSupport.h"

#include <const.h>
#include <types.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern void klog_print(char *str);
extern void klog_print_hex(memaddr);
extern void klog_print_dec(int);

unsigned int current_sp;

pcb_t *sst_pcbs[UPROCMAX];
pcb_t *sup_init_pcb;
pcb_t *swap_mutex;
pcb_t *mutex_holder;

support_t uproc_supports[UPROCMAX];

// TODO: mettere sta roba in util?
pcb_t *create_process(state_t *state, support_t *support) {
  pcb_t *p;
  ssi_create_process_t ssi_create_process = {
      .state = state,
      .support = support,
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

void terminate_process(pcb_t *arg) {
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)arg,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

void acquire_swap_mutex() {
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex, 0, 0);
}

void release_swap_mutex() {
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
}

void p3test() {
  {
    /* state_t s; */
    /* LDST(&s); */
    RAMTOP(current_sp);
    current_sp -= 3 * FRAMESIZE;
  }
  /**
   * Init della Swap Pool Table
   */
  initSwapStructs();

  /**
   * Iniziallizzazione del processo che garantisce la mutua esclusione sulla
   * Swap Pool Table
   */
  state_t swap_mutex_state;
  swap_mutex_state.reg_sp = current_sp;
  swap_mutex_state.pc_epc = (memaddr)swapMutexHandler;
  swap_mutex_state.status = ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  current_sp -= FRAMESIZE;

  swap_mutex = create_process(&swap_mutex_state, NULL);

  int max_uprocs = 8;
  for (int i = 0; i < max_uprocs; i++) {
    int asid = i + 1;

    /*
     * Iniziallizzazione dello stato del processo SST
     */
    state_t sst_state;
    sst_state.reg_sp = current_sp;
    sst_state.pc_epc = sst_state.reg_t9 = (memaddr)sst_handler;
    sst_state.status = ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
    sst_state.entry_hi = asid << ASIDSHIFT;
    current_sp -= FRAMESIZE;

    /*
     * Iniziallizzazione della Support Structure dell'U-PROC
     */
    support_t *sst_support = &uproc_supports[i];
    sst_support->sup_asid = asid;

    /* Init context delle eccezioni PageFault */
    sst_support->sup_exceptContext[PGFAULTEXCEPT].pc =
        (memaddr)TLB_ExceptHandler;
    sst_support->sup_exceptContext[PGFAULTEXCEPT].status =
        ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
    sst_support->sup_exceptContext[PGFAULTEXCEPT].stackPtr = current_sp;
    current_sp -= FRAMESIZE;

    /* Init context delle eccezioni del livello supporto */
    sst_support->sup_exceptContext[GENERALEXCEPT].pc =
        (memaddr)sup_ExceptionHandler;
    sst_support->sup_exceptContext[GENERALEXCEPT].status =
        ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
    sst_support->sup_exceptContext[GENERALEXCEPT].stackPtr = current_sp;
    current_sp -= FRAMESIZE;

    /* Init della Page Table */
    for (int i = 0; i < (USERPGTBLSIZE - 1); i++) {
      sst_support->sup_privatePgTbl[i].pte_entryHI =
          KUSEG + (i << VPNSHIFT) + (asid << ASIDSHIFT);
      sst_support->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
    }
    sst_support->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
        (USERSTACKTOP - PAGESIZE) + (asid << ASIDSHIFT);
    sst_support->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;

    sst_pcbs[i] = create_process(&sst_state, sst_support);
  }

  /*
   * Aspetto i messaggi di fine test
   */
  for (int i = 0; i < max_uprocs; i++) {
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sst_pcbs[i], 0, 0);
  }

  /*
   * InstatiatorProcess termina dopo la conclusione dei test
   */
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

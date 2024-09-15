#include "headers/initProc.h"

#include "headers/common.h"
#include "headers/sst.h"
#include "headers/sysSupport.h"
#include "headers/vmSupport.h"

#include <umps/cp0.h>
#include <umps/libumps.h>

/*
 * Indirizzo del frame da allocare durante l'init degli U-PROC
 */
unsigned int current_sp;

/*
 * PCB's dei processi SST che verranno creati
 */
pcb_t *sst_pcbs[UPROCMAX];

/*
 * PCB dell'InstatiatorProcess
 */
pcb_t *test_pcb;

/*
 * Strutture di Supporto degli U-PROC
 */
support_t uproc_supports[UPROCMAX];

static void init_sst_state(int, state_t *);
static void init_uproc_support(int, support_t *);
static void initSwapMutexPcb();

void p3test() {
  int max_uprocs = UPROCMAX;

  {
    RAMTOP(current_sp);
    current_sp -= 3 * FRAMESIZE; // first 2 frames are used by ssi and test_pcb
  }

  initSwapStructs();

  initSwapMutexPcb();

  /*
   * Inizializzo gli stati e supporti degli U-PROC e faccio partire gli SST
   */
  for (int i = 0; i < max_uprocs; i++) {
    int asid = i + 1;

    state_t sst_state;
    init_sst_state(asid, &sst_state);

    support_t *sst_support = &uproc_supports[i];
    init_uproc_support(asid, sst_support);

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
  terminate_process(NULL);
}

/*
 * Funzione che inizializza lo stato passato come argomento allo scopo di usarlo
 * nella creazione di un processo SST. Venogno impostati il PC, lo status e
 * l'entry_hi. Quest'ultima verrà usata durante l'init dell'U-PROC
 */
static void init_sst_state(int asid, state_t *s) {
  s->reg_sp = current_sp;
  s->pc_epc = s->reg_t9 = (memaddr)sst_handler;
  s->status = ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  s->entry_hi = asid << ASIDSHIFT;
  current_sp -= FRAMESIZE;
}

/*
 * Funzione che inizializza la Support Structure passata come argomento con lo
 * scopo di usarlo durante durante la creazione dell'U-PROC stesso. Vengono
 * impostati il context (sia PageFault che GeneralException) e la PageTable
 */
static void init_uproc_support(int asid, support_t *s) {
  s->sup_asid = asid;

  /* Init context delle eccezioni PageFault */
  s->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)TLB_ExceptHandler;
  s->sup_exceptContext[PGFAULTEXCEPT].status =
      ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  s->sup_exceptContext[PGFAULTEXCEPT].stackPtr = current_sp;
  current_sp -= FRAMESIZE;

  /* Init context delle eccezioni del livello supporto */
  s->sup_exceptContext[GENERALEXCEPT].pc = (memaddr)sup_ExceptionHandler;
  s->sup_exceptContext[GENERALEXCEPT].status =
      ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  s->sup_exceptContext[GENERALEXCEPT].stackPtr = current_sp;
  current_sp -= FRAMESIZE;

  /* Init della Page Table */
  for (int i = 0; i < (USERPGTBLSIZE - 1); i++) {
    s->sup_privatePgTbl[i].pte_entryHI =
        KUSEG + (i << VPNSHIFT) + (asid << ASIDSHIFT);
    s->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
  }
  s->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
      (USERSTACKTOP - PAGESIZE) + (asid << ASIDSHIFT);
  s->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
}

/*
 * Funzione che inizializza e crea il processo che gestisce la mutua esclusione
 * della Swap Pool
 */
static void initSwapMutexPcb() {
  state_t swap_mutex_state;
  swap_mutex_state.reg_sp = current_sp;
  swap_mutex_state.pc_epc = (memaddr)swapMutexHandler;
  swap_mutex_state.status = ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  current_sp -= FRAMESIZE;

  sw_mutex_pcb = create_process(&swap_mutex_state, NULL);
}

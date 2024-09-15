#include "headers/vmSupport.h"

#include "../phase2/headers/devices.h"
#include "headers/common.h"
#include "headers/sysSupport.h"

#include <types.h>
#include <umps/arch.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

/*
 * Indice del prossimo frame della Swap Pool da utilizzare nel caso in cui siano
 * tutti pieni
 */
int sw_idx = 0;

/*
 * PCB del processo che garantisce la mutua esclusione della Swap Pool Table
 */
pcb_t *sw_mutex_pcb;

/*
 * PCB del processo che è in possesso della mutua esclusione della Swap Pool
 * Table
 */
pcb_t *mutex_holder;

/*
 * Tabella per la gestione dei frame della Swap Pool Table
 */
swap_t swap_table[SWAP_POOL_SIZE];

static void TLB_PageFaultHandler(support_t *excp_support);
static void update_tlb(pteEntry_t);
static void flashdev_readblk(int, unsigned int, unsigned int);
static void flashdev_writeblk(int, unsigned int, unsigned int);
static int next_swap_entry();
static inline void IE_OFF();
static inline void IE_ON();

/*
 * Gestore delle eccezioni TLB-Invalid e TLB-Modification.
 * Le prime tratta come pagefault, mentre le seconde come trap.
 */
void TLB_ExceptHandler() {
  support_t *excp_support = get_support_data();
  state_t excp_state = excp_support->sup_exceptState[PGFAULTEXCEPT];

  switch (CAUSE_GET_EXCCODE(excp_state.cause)) {
  case EXC_TLBL:
  case EXC_TLBS:
    TLB_PageFaultHandler(excp_support);
    break;
  case EXC_MOD:
    programTrapHandler(excp_support->sup_asid);
    break;
  default:
    PANIC();
    break;
  }
}

/*
 * Gestore della mutua esclusione delle Swap Pool
 */
void swapMutexHandler() {
  while (TRUE) {
    msg_t *msg;
    mutex_holder = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);

    SYSCALL(SENDMESSAGE, (unsigned int)mutex_holder, 0, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_holder, 0, 0);
  }
}

/*
 * Funzione che inizializza le strutture dedicate alla gestione della Swap Pool.
 * In particolare inizializza la Swap Table segnando come liberi tutti i frame
 * della pool.
 */
void initSwapStructs() {
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    swap_table[i].sw_asid = SW_ENTRY_UNUSED; // -1 for unused frame
    swap_table[i].sw_pageNo = 0;
    swap_table[i].sw_pte = NULL;
  }
}

/*
 * Funzione che segna come inutilizzati i frame della Swap Pool allocati
 * dall'U-PROC specificato
 */
void invalidate_uproc_frames(int asid) {
  acquire_swap_mutex();
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    if (swap_table[i].sw_asid == asid) {
      swap_table[i].sw_asid = SW_ENTRY_UNUSED;
    }
  }
  release_swap_mutex();
}

/*
 * Funzione che gestisce le PageFault: ricava la pagina che è stata richiesta e
 * la carica in RAM in uno dei frame della Swap Pool (ne libera uno se sono
 * tutti occupati) mantendo aggiornate la Swap Pool Table, la Page Table
 * dell'U-PROC che ha scatenato la PageFault e il TLB
 */
static void TLB_PageFaultHandler(support_t *excp_support) {
  int curr_asid = excp_support->sup_asid;
  int frame_idx;
  unsigned int frame_address;
  pteEntry_t *uproc_pte;
  swap_t *sw_entry;
  state_t excp_state = excp_support->sup_exceptState[PGFAULTEXCEPT];
  unsigned int missing_page = get_missing_page(excp_state.entry_hi);

  uproc_pte = &excp_support->sup_privatePgTbl[missing_page];

  acquire_swap_mutex();

  /* Ricavo il frame della Swap Pool che andrò ad allocare */
  frame_idx = next_swap_entry();
  frame_address = SWAP_POOL_BASE + ((frame_idx + 1) * FRAMESIZE);
  sw_entry = &swap_table[frame_idx];

  /*
   * Se il frame è occupato lo segno come non valido e lo scrivo nel backing
   * store dell'U-PROC corrispondente aggiornando il TLB di conseguenza
   */
  if (sw_entry->sw_asid != SW_ENTRY_UNUSED) {
    IE_OFF();
    sw_entry->sw_pte->pte_entryLO &= ~VALIDON;
    update_tlb(*sw_entry->sw_pte);
    IE_ON();

    flashdev_writeblk(sw_entry->sw_asid, frame_address, missing_page);
  }

  /* Leggo la pagina dal backing store dell'U-PROC al frame in RAM */
  flashdev_readblk(curr_asid, frame_address, missing_page);

  /* Aggiorno la Page Table dell'U-PROC*/
  uproc_pte->pte_entryLO &= 0x00000FFF; // cancello il vecchioPFN
  uproc_pte->pte_entryLO |= frame_address | VALIDON;

  /* Aggiorno la SwapPool Table entry e la TLB entry*/
  IE_OFF();
  sw_entry->sw_asid = curr_asid;
  sw_entry->sw_pte = uproc_pte;
  sw_entry->sw_pageNo = uproc_pte->pte_entryHI >> VPNSHIFT;
  update_tlb(*sw_entry->sw_pte);
  IE_ON();

  release_swap_mutex();

  LDST(&excp_state);
}

/*
 * Funzione che aggiorna l'entry specificata del TLB. Nota: ciò avviene se e
 * solo se l'entry è contenuta nel TLB
 */
static void update_tlb(pteEntry_t pte_entry) {
  setENTRYHI(pte_entry.pte_entryHI);
  TLBP();

  if ((getINDEX() & 0x80000000) == 0) { // TRUE iff the TLB contains pte_entry
    setENTRYHI(pte_entry.pte_entryHI);
    setENTRYLO(pte_entry.pte_entryLO);
    TLBWI();
  }
}

/*
 * Funzione che scrive il blocco della RAM specificato nel flash device
 * dell'U-PROC.
 */
static void flashdev_writeblk(int asid, unsigned int ram_blk,
                              unsigned int device_blk) {
  int status;
  dtpreg_t *dev_reg = (dtpreg_t *)DEV_REG_ADDR(FLASHINT, asid - 1);
  dev_reg->data0 = ram_blk;
  status = do_io(&dev_reg->command, (device_blk << 8) + FLASHDEV_WRITEBLK);
  if (status != DEVSTATUS_READY) {
    programTrapHandler(asid);
  }
}

/*
 * Funzione che legge il blocco specificato dal flash device dell'U-PROC al
 * frame della RAM.
 */
static void flashdev_readblk(int asid, unsigned int ram_blk,
                             unsigned int device_blk) {
  int status;
  dtpreg_t *dev_reg = (dtpreg_t *)DEV_REG_ADDR(FLASHINT, asid - 1);
  dev_reg->data0 = ram_blk;
  status = do_io(&dev_reg->command, (device_blk << 8) + FLASHDEV_READBLK);
  if (status != DEVSTATUS_READY) {
    programTrapHandler(asid);
  }
}

/*
 * Funzione che restituisce l'indice del prossime frame della Swap Pool da
 * allocare. Per sceglierlo cerca il primo frame libero. Nel caso in cui
 * dovessero essere tutti occupati, utilizza 'sw_idx' per scegliere il
 * prossimo seguendo la logica RoundRobin
 */
static int next_swap_entry() {
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    if (swap_table[i].sw_asid < 0) {
      return i;
    }
  }
  int i = sw_idx;
  sw_idx = (sw_idx + 1) & SWAP_POOL_SIZE;
  return i;
}

/*
 * Funzione che DISATTIVA gli interrupts. Viene utilizzata per garantire
 * l'atomicità di alcune operazioni
 */
static inline void IE_OFF() { setSTATUS(getSTATUS() & ~STATUS_IEc); }

/*
 * Funzione che ATTIVA gli interrupts. Viene utilizzata per garantire
 * l'atomicità di alcune operazioni
 */
static inline void IE_ON() { setSTATUS(getSTATUS() | STATUS_IEc); }

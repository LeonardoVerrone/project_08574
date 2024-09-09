#include "headers/vmSupport.h"

#include "../phase2/headers/util.h"

#include <const.h>
#include <types.h>
#include <umps/arch.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

extern pcb_t *ssi_pcb;
extern pcb_t *mutex_holder;

extern support_t *get_support_data();
extern void acquire_swap_mutex();
extern void release_swap_mutex();
extern void programTrapHandler();

extern void klog_print(char *str);
extern void klog_print_hex(unsigned int);
extern void klog_print_dec(int);

swap_t swap_table[SWAP_POOL_SIZE];
int idx_swap; // entry index of the last allocated frame

void initSwapStructs() {
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    swap_table[i].sw_asid = -1; // -1 for unused frame
    swap_table[i].sw_pageNo = 0;
    swap_table[i].sw_pte = NULL;
  }

  idx_swap = 0;
}

swap_t *next_table_entry() {
  idx_swap = (idx_swap + 1) % (2 * UPROCMAX);
  return &swap_table[idx_swap];
}

unsigned int block_address(unsigned int block_number) {
  return 0x20020000 + (block_number * FRAMESIZE);
}

void flashdev_IO(int flashdev_command, int dev_number, unsigned int ram_block,
                 unsigned int other_block) {
  unsigned int status;
  /*
   * Costruisco il comando da mandare all'SST
   */
  dtpreg_t *dev_reg = (dtpreg_t *)DEV_REG_ADDR(4, dev_number);
  unsigned int command =
      (other_block << 8) + flashdev_command; // TODO: creare costanti
  ssi_do_io_t do_io = {.commandAddr = &dev_reg->command,
                       .commandValue = command};
  ssi_payload_t payload = {.service_code = DOIO, .arg = &do_io};

  /*
   * Scrivo dentro a DATA0 il blocco RAM da copiare
   */
  dev_reg->data0 = ram_block;

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

  if (status != 1) {
    // TODO: trap
  }
}

void update_tlb(pteEntry_t pte_entry) {
  TLBCLR();

  setENTRYHI(pte_entry.pte_entryHI);
  setENTRYLO(pte_entry.pte_entryLO);
  TLBWR();
}

unsigned int flashdev_block(unsigned int missing_page) {
  return (missing_page << VPNSHIFT) - KUSEG;
}

void TLB_PageFaultHandler(support_t *curr_proc_sup, state_t *saved_excp_state) {
  acquire_swap_mutex();

  unsigned int missing_page = ENTRYHI_GET_VPN(saved_excp_state->entry_hi);
  if (missing_page > MAXPAGES) { // TRUE iff the missing page is the stack page
    missing_page = MAXPAGES - 1;
  }

  swap_t *swap_entry = next_table_entry();

  if (swap_entry->sw_pageNo > 0) { // check if the frame is occupied
    swap_entry->sw_pte->pte_entryLO &= !VALIDON;
    update_tlb(*swap_entry->sw_pte);
    flashdev_IO(3, swap_entry->sw_asid - 1, block_address(idx_swap),
                swap_entry->sw_pageNo);
  }
  /* unsigned int flshdev_block = flashdev_block(missing_page); */
  flashdev_IO(2, curr_proc_sup->sup_asid - 1, block_address(idx_swap),
              missing_page);

  /*
   * Aggiorno SwapPool Table, U-PROC's Page Table e TLB
   */
  /* Finding the entry in U-PROCS's Page Table */
  pteEntry_t *uproc_tbe = &curr_proc_sup->sup_privatePgTbl[missing_page];

  /* Updating the U-PROC's Page Table entry */
  uproc_tbe->pte_entryLO &= 0xFFFFFFFF >> 20; // erase the old PFN
  uproc_tbe->pte_entryLO |= block_address(idx_swap) | VALIDON;

  /* Updating the SwapPool Table entry */
  swap_entry->sw_asid = curr_proc_sup->sup_asid;
  swap_entry->sw_pte = uproc_tbe;

  update_tlb(*swap_entry->sw_pte);

  release_swap_mutex();

  LDST(saved_excp_state);
}

void TLB_ExceptHandler() {
  support_t *curr_proc_sup = get_support_data();
  state_t *saved_excp_state = &curr_proc_sup->sup_exceptState[PGFAULTEXCEPT];

  switch (CAUSE_GET_EXCCODE(saved_excp_state->cause)) {
  case EXC_TLBL:
  case EXC_TLBS:
    TLB_PageFaultHandler(curr_proc_sup, saved_excp_state);
    break;
  case EXC_MOD:
    programTrapHandler();
    break;
  default:
    // FIXME: trap?
    break;
  }
}

void swapMutexHandler() {
  while (TRUE) {
    msg_t *msg;
    mutex_holder = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);

    SYSCALL(SENDMESSAGE, (unsigned int)mutex_holder, 0, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_holder, 0, 0);
  }
}

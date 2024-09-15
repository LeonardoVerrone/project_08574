# MicroPandOS su umps3

## Phase 2
### Struttura del progetto
I moduli `initial.c`, `exception.c`, `interrupts.c`, `scheduler.c`, `ssi.c` sono stati implementati in maniera piuttosto standard attenendosi il più possibile alle specifiche.

È stato creato il modulo `devices.c` per implementare alcuni metodi di assistenza nella gestione dei (sub)devices mediante l'utilizzo dei relativi registri.

Infine sono stati creato i moduli `globals.c` e `util.c` di supporto ai rimanenti.

### Scelte implementative
#### Gestione coda dei devices
All'interno di `globals.c` viene inizializzato l'array `waiting_for_IO[]` che viene utilizzato per salvare i pcb che sono in attesa del completamento di una operazione IO. In particolare in ogni indice viene salvato il pcb che è in attesa di un certo device che viene identificato dell'indice stesso tramite la formula `dev_class * 8 + dev_number`.

Quando viene fatta una richiesta DoIO verso un device che sta già elaborando un'altro richiesta, allora durante l'elaborazione da parte dell'SSI della richiesta stessa, essa viene inoltrato nuovamente all'SSI, in questo modo il processo richiedente rimane in attesa fino a quando il device non si sarà liberato. Vedere da `ssi.c#SSI_doIO`.

## Phase 3
### Struttura del progetto
Sono stati creati i moduli `initProc.c`, `sst.c`, `sysSupport.c`, `sysSupport.c`, `vmSupport.c` e `common.c` per la realizzazione del Livello Supporto. I moduli `initProc.c`, `sst.c`, `sysSupport.c`, `sysSupport.c` e `vmSupport.c`, sono stati realizzati in maniera più attinente possibile alle specifiche, mentre il modulo `common.c` contiene funzioni di utili alle procedure del Livello Supporto (per la maggior parte si tratta di metodi per l'esecuzione di richieste all'SSI).

### InstatiatorProcess
L'InstatiatorProcess è quel processo che inizializza le strutture del Livello Supporto e che successivamente crea i processi degli U-proc (e i relativi SST). Successivamente attende da ogni SST un messaggio che comunica la terminazione dello stesso. L'InstatiatorProcess termina solo quando avrà ricevuto conferma di avvenuta terminazione da parte di ogni SST creato.

Il comportamento dell'InstatiatorProcess è implementato dal modulo `initProc.c`.

#### Inizializzazione delle struttue del Livello Supporto
Si tratta di inizializzare le strutture a supporto della Swap Pool:
- la Swap Pool Table
- il processo che garantisce la mutua esclusione sulla Swap Pool Table

Il primo compito viene svolto dalla procedura `vmSupport.c#initSwapStructs`:
```
void initSwapStructs() {
  for (int i = 0; i < SWAP_POOL_SIZE; i++) {
    swap_table[i].sw_asid = SW_ENTRY_UNUSED; // -1 for unused frame
    swap_table[i].sw_pageNo = 0;
    swap_table[i].sw_pte = NULL;
  }
}
```

Mentre il secondo compito viene svolto dalla procedura `initProc.c#initSwpaMutex`:
```
static void initSwapMutexPcb() {
  state_t swap_mutex_state;
  swap_mutex_state.reg_sp = current_sp;
  swap_mutex_state.pc_epc = (memaddr)swapMutexHandler;
  swap_mutex_state.status = ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  current_sp -= FRAMESIZE;

  sw_mutex_pcb = create_process(&swap_mutex_state, NULL);
}
```

#### Creazione degli U-proc
L'InstatiatorProcess ha il compito di creare i processi SST e configurarli in maniera tale che all'inizio della loro esecuzione creeranno a loro volta un processo U-proc a testa. Per ottenere ciò bisogna prima inizializzare lo **stato degli SST**, e poi le **strutture di Supporto degli U-proc**.  Infatti ogni processo SST viene creato specificando il suo stato appena inizializzato e il Supporto dell'U-proc figlio dell'SST.

L'inizializzazione dello stato di ogni SST viene implementata dalla funzione `initProc.c#init_sst_state`:
```
static void init_sst_state(int asid, state_t *s) {
  s->reg_sp = current_sp;
  s->pc_epc = s->reg_t9 = (memaddr)sst_handler;
  s->status = ALLOFF | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;
  s->entry_hi = asid << ASIDSHIFT;
  current_sp -= FRAMESIZE;
}
```

Mentre l'inizializzazione di ogni Struttura di Supporto degli U-proc viene implementata dalla funzione `initProc.c#init_uproc_support`:
```
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
```
Per ogni struttura di Supporto bisogna inizializzare i `context` che verrà utilizzati durante la procedura `exceptions.c#passUpOrDie` per passare il controllo al gestore delle eccezioni opportuno. Infatti se l'eccezione da passare è una eccezione TLB (TLB-Invalid o TLB-Modification) verrà chiamato il `vmSupport.c#TLB_ExceptHandler`, in tutti gli altri casi verrà chiamato il gestore `sysSupport.c#sup_ExceptionHandler`.

Inoltre viene inizializzata la Page Table dell'U-proc. In particolare vengono specificati gli indirizzi virtuali delle pagine corrispondenti ai frame del backing store dell'U-proc.

#### Terminazione dell'InstatiatorProcess
Una volta creati gli SST, l'InstatiatorProcess, rimane in attesa dei messagi di terminazione da parte degli SST.
```
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
```

### SST (Service System Thread)
Il processo SST si occupa di servire all'U-proc figlio dei servizi aggiuntivi. 

Di fatto ogni processo SST esegue una sola volta la procedura `sst.c#sst_handler` che prima crea l'U-proc figlio e successivamente si mette in ascolto per ricevere ed eseguire le sue richieste.

#### Creazione dell'U-proc figlio
Per creare l'U-proc figlio è necessario specificare lo stato e la struttura di supporto che dovrà avere.

Lo stato viene inizializzato partendo dallo stato dell'SST specificando lo Stack Pointer, il Program Counter e lo Status.

Invece per quanto riguarda la struttura di supporto, viene utilizzata quella dell'SST.

Tutto ciò avviene nella procedura `sst.c#create_uproc`:
```
static pcb_t *create_uproc() {
  /*
   * Inizializzo lo stato partendo da quello dell'SST dove ho già impostato
   * l'EntryHI.ASID corretto
   */
  state_t uproc_state;
  STST(&uproc_state);
  uproc_state.reg_sp = USERSTACKTOP;
  uproc_state.pc_epc = uproc_state.reg_t9 = UPROCSTARTADDR;
  uproc_state.status =
      ALLOFF | STATUS_KUp | STATUS_IEp | STATUS_IM_MASK | STATUS_TE;

  return create_process(&uproc_state, get_support_data());
}
```
***NOTA:*** durante l'inizializzazione dello stato dell'SST ho già configurato il campo `EntryHI.ASID`.

#### Soddisfacimento delle richieste
Dopo aver creato l'U-proc figlio, il processo SST, si mette in attesa di richieste da soddisfare, tale funzionalità è implementata nella seguente porzione di codice della procedura `sst.c#sst_handler`:
```
  while (TRUE) {
    ssi_payload_t *payload = NULL;
    pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)uproc_pcb,
                                     (unsigned int)&payload, 0);
    int sender_asid = sender->p_supportStruct->sup_asid;
    switch (payload->service_code) {
    case GET_TOD:
      unsigned int tod;
      STCK(tod);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, tod, 0);
      break;
    case TERMINATE:
      programTrapHandler(sender->p_supportStruct->sup_asid);
      break;
    case WRITEPRINTER:
      print(PRNT_CLASS, sender_asid - 1, (sst_print_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    case WRITETERMINAL:
      print(TERM_CLASS, sender_asid - 1, (sst_print_t *)payload->arg);
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
      break;
    default:
      PANIC();
      break;
    }
  }
```
***NOTA:*** per terminare il processo simulo una trap. Infatti all'interno di `sysSupport.c#programTrapHandler` viene terminato l'SST (e quindi l'U-proc) correttamente, inviando un messaggio all'InstatiatorProcess, e pulendo i frame della Swap Pool occupati dall'U-proc.

### Gestione delle eccezioni
Il Livello di Supporto deve gestire più tipi di eccezioni, che si dividono in due categorie: le eccezioni TLB, e le eccezioni non-TLB. 

#### Gestione delle eccezioni TLB
Le eccezioni TLB possono essere di tre tipi:
- ***TLB-Refill***, non sono propriamento un'eccezione, piuttosto un evento che si verifica durante la traduzione di un indirizzo virtuale quando non è presente nessuna entry nel TLB che corrisponde alla pagina richiesta. Il gestore degli eventi TLB-Refill è la procedura `initial.c#uTLB_RefillHandler`. 
- ***TLB-Invalid*** exception, scatenate durante la traduzione di un indirizzo virtuale, quando la TLB entry corrispondente alla pagina richiesta è segnata come invalida. Questa eccezione viene trattata come una Page Fault, e il gestore è la procedura `vmSupport.c#TLB_ExceptHandler` che poi passerà il controllo alla procedura `vmSupport.c#TLB_PageFaultHandler`.
- ***TLB-Modification*** exception, scatenate quando viene tentata la scrittura su una pagina che però è segnata come readonly. Il gestore è la procedura `vmSupport.c#TLB_ExceptHandler` la quale tratterà l'eccezione come una vera e propria trap.

Il gestore degli eventi TLB-Refill è implementato dalla procedura `initial.c#uTLB_RefillHandler`, e si occupa di ricvare la pagina richiesta e caricare l'entry dell U-proc's Page Table corrispondente nel TLB:
```
void uTLB_RefillHandler() {
  state_t *excp_state = (state_t *)BIOSDATAPAGE;
  support_t *excpt_sup = current_process->p_supportStruct;

  pteEntry_t *tlb_entry =
      &excpt_sup->sup_privatePgTbl[get_missing_page(excp_state->entry_hi)];

  setENTRYHI(tlb_entry->pte_entryHI);
  setENTRYLO(tlb_entry->pte_entryLO);
  TLBWR();
  LDST(excp_state);
}
```

Il gestore delle eccezioni TLB è la procedura `vmSupport.c#TLB_ExceptHandler`:
```
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
```

Infine il gestore delle eccezioni `TLB-Invalid` è implementato dalla procedura `vmSupport.c#TLB_PageFaultHandler`, e si occupa di ricavare la pagina richiesta durante la traduzione dell'indirizzo virtuale per poi caricarla all'interno di un frame della Swap Pool. Se nessun frame è libero allora ne sceglie uno che verrà scritto nel backing store dell'U-proc corrispondente, rimuovendolo dalla Swap Pool e quindi liberandolo. Durante queste operazioni vengono aggiornate le strutture Swap Pool Table, TLB e U-proc's Page Table con il fine di mantenere le corrispondenze consistenti.
```
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
```
***NOTA:*** durante la maggior parte delle operazioni, la procedura acquisisce la mutua esclusione sulla Swap Pool Table, cosicchè non possano esserci altri processi che la modifichino in contemporanea. Inoltre per garantire l'atomicità delle operazioni di aggiornamento del TLB e di aggiornamento della Swap Pool Table, vengono disattivati gli interrupt prima di eseguirle per riattivarli una volta terminate.

#### Gestione delle eccezioni non-TLB
Il gestore delle eccezioni è implementato dalla procedura `sysSupport.c#sup_ExceptionHandler`:
```
void sup_ExceptionHandler() {
  support_t *excp_support = get_support_data();

  const int cause_code =
      CAUSE_GET_EXCCODE(excp_support->sup_exceptState[GENERALEXCEPT].cause);

  if (cause_code == EXC_SYS) {
    sup_SyscallHandler(excp_support);
  } else {
    programTrapHandler(excp_support->sup_asid);
  }
}
```
Il gestore distingue le eccezioni System Call da tutte le altre. Nel primo caso viene chiamato il relativo gestore `sysSupport.c#sup_SyscallHandler`, mentre in tutti gli altri casi le eccezioni vengono trattate come Program Trap.

Il gestore delle **System Call** del livello Supporto è implementato dalla procedura `sysSupport.c#sup_SyscallHandler`:
```
static void sup_SyscallHandler(support_t *excp_support) {
  state_t *excp_state = &excp_support->sup_exceptState[GENERALEXCEPT];
  pcb_t *dest = (pcb_t *)excp_state->reg_a1;
  const unsigned int payload = excp_state->reg_a2;

  switch (excp_state->reg_a0) {
  case SENDMSG:
    if (dest == PARENT) {
      dest = sst_pcbs[excp_support->sup_asid - 1];
    }
    SYSCALL(SENDMESSAGE, (unsigned int)dest, payload, 0);
    break;
  case RECEIVEMSG:
    SYSCALL(RECEIVEMESSAGE, (unsigned int)dest, payload, 0);
    break;
  default:
    programTrapHandler(excp_support->sup_asid);
    return;
  }

  excp_state->pc_epc += WORDLEN;
  LDST(excp_state);
}
```
Vengono gestite le due System Call USYS1 e USYS2 che di fatto sono dei wrapper delle eccezioni SYS1 e SYS2.

Le **Program Trap** invece vengono gestite dalla procedura `sysSupport.c#programTrapHandler`, che si occupa di terminare il processo inviando un messaggio all'InstatiatorProcess e segnando come invalidi tutti i frame della Swap Pool allocati dall'U-proc.
```
void programTrapHandler(int asid) {
  /* Rilascio la mutua esclusione della Swap Pool (se necessario) */
  if (mutex_holder->p_supportStruct->sup_asid == asid) {
    release_swap_mutex();
  }

  /* Segno come inutilizzati i frame della Swap Pool che occupavo */
  invalidate_uproc_frames(asid);

  /* Avviso il processo di test che sto terminando */
  SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0);

  terminate_process(NULL);
}
```

### Swap Pool
La Swap Pool consiste in un insieme di frame in RAM utilizzati di supporto alla memoria virtuale. Infatti lo scopo dei frame della Swap Pool è quello di mantenere in memoria i frame dei backing store degli U-proc. 

#### Swap Pool Table
La Swap Pool Table è tabella che contiene lo stato dei frame della Swap Pool, in particolare l'*i*-esima riga della tabella fa riferimento all'*i*-esimo frame della Swap Pool.

Viene implementata come array di `struct swap_t` e si concretizza nella variabile `vmSupport.c#swap_table[]`.


## Build:
Per compilare:
```bash
make
```



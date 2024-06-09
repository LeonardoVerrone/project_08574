# MicroPandOS su umps3

## Phase 2
### Struttura del progetto
I moduli `initial.c`, `exception.c`, `interrupts.c`, `scheduler.c`, `ssi.c` sono stati implementati in manierea piuttosto standard attenendosi il più possibile alle specifiche.

È stato creato il modulo `devices.c` per implementare alcuni metodi di assistenza nella gestione dei (sub)devices mediante l'utilizzo dei relativi registri.

Infine sono stati creato i moduli `globals.c` e `util.c` di supporto ai rimanenti.

### Scelte implementative
#### Gestione coda dei devices
All'interno di `globals.c` viene inizializzato l'array `waiting_for_IO[]` che viene utilizzato per salvare i pcb che sono in attesa del completamento di una operazione IO. In particolare in ogni indice viene salvato il pcb che è in attesa di un certo device che viene identificato dell'indice stesso tramite la formula `dev_class * 8 + dev_number`.

Quando viene fatta una richiesta DoIO verso un device che sta già elaborando un'altro richiesta, allora durante l'elaborazione da parte dell'SSI della richiesta stessa, essa viene inoltrato nuovamente all'SSI, in questo modo il processo richiedente rimane in attesa fino a quando il device non si sarà liberato. Vedere da `ssi.c#SSI_doIO`.


## Build:
Per compilare:
```bash
make
```



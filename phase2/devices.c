#include "headers/devices.h"

#include <const.h>
#include <umps/arch.h>
#include <umps/cp0.h>
#include <umps/libumps.h>

/*
 * Restituisce la linea dell'interrupt attivo con priorità maggiore, la deduce
 * dal registro CAUSE
 */
int get_intline_from_cause() {
  for (int i = 0; i < NUMBER_OF_INTLINES; i++)
    if (getCAUSE() & CAUSE_IP(i))
      return i;
  PANIC();
  return -1;
}

/*
 * Restituisce il numero di device che ha attivato l'interrupt sulla linea
 * indicata. Per farlo va conrollare la Interrupting Devices Bit Map
 */
int get_dev_number(int intline) {
  unsigned int *line_address = (unsigned int *)CDEV_BITMAP_ADDR(intline);
  for (int i = 0; i < NUMBER_OF_DEVICES; i++) {
    if ((*(line_address) >> i) & 0x1) {
      return i;
    }
  }
  PANIC();
  return -1;
}

/*
 * Dato l'indirizzo del comando, restituisce il device a cui fa riferimento
 */
void get_device_id(unsigned int command_address, device_id_t *device_id) {
  // parto dal basso e scorro tutti i device registers fino a trovare quello che
  // contiene il comando passato come argomento
  for (int dev_class = 0; dev_class < DEVINTNUM; dev_class++) {
    for (int dev_number = 0; dev_number < DEVPERINT; dev_number++) {
      if ((dev_class, dev_number) <= command_address &&
          command_address <=
              compute_devreg_addr(dev_class, dev_number) + DEVREGSIZE) {
        device_id->dev_class = dev_class;
        device_id->dev_number = dev_number;
        return;
      }
    }
  }
}

/*
 * Restituisce il device register del corrispondente alla coppia di
 * classe-numero.
 *
 * NOTE: Come umps/arch.h#DEV_REG_ADD(line, dev), ma con la classe al posto
 * della linea dell'interrupt
 */
unsigned int compute_devreg_addr(int dev_class, int dev_number) {
  return DEV_REG_START + (dev_class * DEVREGSIZE * DEVPERINT) +
         (dev_number * DEVREGSIZE);
}

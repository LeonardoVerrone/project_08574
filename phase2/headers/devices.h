#ifndef DEVICES_HEADER
#define DEVICES_HEADER

#include <umps/const.h>

#define NUMBER_OF_DEVICES (DEVINTNUM * DEVPERINT) + DEVINTNUM

/*
 * Interrupt lines
 */
#define NUMBER_OF_INTLINES 8
#define INTPROC_INT 0
#define PLT_INT 1
#define LOCALTIMER_INT 2
#define DISKDEV_INT 3
#define FLASHDEV_INT 4
#define NETDEV_INT 5
#define PRINTERDEV_INT 6
#define TERMDEV_INT 7

typedef struct device_id_t {
  int dev_class;
  int dev_number;
} device_id_t;

int get_intline_from_cause();
int get_dev_number(int intline);
void get_device_id(unsigned int command_address, device_id_t *device_id);
unsigned int compute_reg_address(int dev_class, int dev_number);

#endif // !DEVICES_HEADER

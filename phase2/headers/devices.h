#ifndef DEVICES_HEADER
#define DEVICES_HEADER

#include <umps/const.h>

/*
 * Interrupt lines
 */
#define NUMBER_OF_INTLINES 8
#define INTPROC_INT 0
#define PLT_INT 1
#define LOCALTIMER_INT 2
#define DISKDEV_INT 3
#define FLASHDEV_INT 4
#define NETWDEV_INT 5
#define PRINTERDEV_INT 6
#define TERMDEV_INT 7

/*
 * Device classes
 */
#define DISK_CLASS 0
#define FLASH_CLASS 1
#define NETW_CLASS 2
#define PRNT_CLASS 3
#define TERM_CLASS 4

/*
 * Device STATUS codes
 */
#define DEVSTATUS_READY 1

/*
 * Device commands
 */
#define FLASHDEV_READBLK 2
#define FLASHDEV_WRITEBLK 3

typedef struct device_id {
  int dev_class;
  int dev_number;
} device_id_t;

int get_intline_from_cause();

int get_dev_number(int intline);

void get_device_id(unsigned int command_address, device_id_t *device_id);

unsigned int compute_devreg_addr(int dev_class, int dev_number);

#endif // !DEVICES_HEADER

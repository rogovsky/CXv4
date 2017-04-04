#ifndef __CANDRV_H
#define __CANDRV_H

#define CANDRV_VERSION "3.04 beta"

#define CAN_MAJOR 121

typedef struct{
  unsigned char data[8];
  unsigned int id;
  unsigned char len;
  unsigned char flags;
  long sec;
  long usec;
} can_msg;

typedef can_msg can_msg_wt; /* for backward compatibility */

/*
 *  predefined signal numbers
 */
/* #define CAN_SIG_TX    SIGRTMIN */
/* #define CAN_SIG_RX    SIGRTMIN+1 */
/* #define CAN_SIG_BOFF  SIGRTMIN+2 */
#define CAN_SIG_TX    30
#define CAN_SIG_RX    10
#define CAN_SIG_BOFF  16

struct can_baud {
  int baud;
  unsigned char btr0;
  unsigned char btr1;
};

struct can_bchip_status {
  int board_type;
  int irq;
  unsigned long baseaddr;
  int baud;
  
  /* this register are readable in all mode */
  unsigned char ctl;
  unsigned char stat;
  unsigned char interrupt;
  unsigned char clkdiv;
  /* this register are readable in reset mode only */
  unsigned char acode;
  unsigned char amask;
  unsigned char btr0;
  unsigned char btr1;
  unsigned char outctl;
};

/*
//Commands for CAN Driver ioctl() call.
*/
#define CAN_IOC_MAGIC 'u'

#define CAN_SET_BAUDRATE       _IO(CAN_IOC_MAGIC, 0)
#define CAN_SET_ACODE          _IO(CAN_IOC_MAGIC, 1)
#define CAN_SET_AMASK          _IO(CAN_IOC_MAGIC, 2)
#define CAN_SET_OUTCTL         _IO(CAN_IOC_MAGIC, 3)
#define CAN_STOP_CHIP          _IO(CAN_IOC_MAGIC, 4)
#define CAN_START_CHIP         _IO(CAN_IOC_MAGIC, 5)
#define CAN_GET_STATUS         _IO(CAN_IOC_MAGIC, 6)
#define CAN_GET_FULL_STATUS    _IO(CAN_IOC_MAGIC, 7)
#define CAN_SW_BOFF_SIG        _IO(CAN_IOC_MAGIC, 8)
#define CAN_SW_RX_SIG          _IO(CAN_IOC_MAGIC, 9)
#define CAN_SW_TX_SIG          _IO(CAN_IOC_MAGIC, 10)

/*
// Baud rates of CAN chip (recommended by CiA)
*/
#define BAUD_MANUAL 0 /* to distinguish CiA and user bauds */
#define BAUD_1M   1
#define BAUD_800K 2
#define BAUD_500K 3
#define BAUD_250K 4
#define BAUD_125K 5
#define BAUD_50K  6
#define BAUD_20K  7
#define BAUD_10K  8


#define CAN_FIFO_SIZE     250
#define CAN_DATA_SIZE     8
#define CAN_NUM_BOARDS    8

/*
 *  Symbolyc names for CAN adapter types 
 */
#define UNKNOWN           0
#define CAN_BUS_ISA       1
#define CAN_BUS_MICROPC_1 2 /* first chip of CAN-bus-MicroPC */
#define CAN_BUS_MICROPC_2 3 /* second chip of CAN-bus-MicroPC */
#define CAN_BUS_PCI_1     4 /* first chip of CAN-bus-PCI */
#define CAN_BUS_PCI_2     5 /* second chip of CAN-bus-PCI */


#define CAN_WRITE_TIMEOUT 50000 /* 0.05 sec. for timeout */

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <asm/siginfo.h>
#include <asm/param.h>

/* vendor id and device id for CAN-bus-PCI */
#define CAN_bus_PCI_VendorID 0x10b5
#define CAN_bus_PCI_DeviceID 0x2715 /*0x9050*/

#define CAN_MSG_SIZE (sizeof(can_msg))

struct can_fifo {
  char q[CAN_FIFO_SIZE][CAN_MSG_SIZE];
  long length;
  long count;
  long sloc;      /* next store elem. adress */
  long rloc;      /* next retrieve elem. adress */
  spinlock_t lock;
};

struct can_dev{
  spinlock_t lock;

  /* io resources */
  int irq;
  unsigned long base_address;
  unsigned long io;
  unsigned long reset_area;
  
  int minor;

  /* Driver status and control flags */
  unsigned char opened;
  unsigned char baud;
  unsigned long overruns;

  unsigned char sig_bus_off;
  unsigned char sig_receive;
  unsigned char sig_transmit;

  /* proc fs members */
  struct proc_dir_entry* pentry;

  /* for interaction with user-level process */
  struct task_struct* task;
  wait_queue_head_t wait_rq;   /* pocesses that wonts read */
  wait_queue_head_t wait_wq;   /* pocesses that wonts write */
  
  /* DATA storage */
  struct can_fifo rc_fifo;
  struct can_fifo tr_fifo;

  struct pci_dev *pdev;        /* pci device struct for CAN-bus-PCI */
  int pci_iocfg_regnum;
  int type;                    /* type of adapter that includes our chip */
};

/*
 * Symbolic names of 82c200 registers
 */
#define CTRL_REG   0
#define CMD_REG    1
#define STAT_REG   2
#define INT_REG    3
#define ACODE_REG  4
#define AMASK_REG  5
#define BTR0_REG   6
#define BTR1_REG   7
#define OC_REG     8
#define TRBUF_BASE 10
#define RCBUF_BASE 20
#define CLKDIV_REG 31

/*
// Interrupts
*/
#define OIE 0x10 /* overrun */
#define EIE 0x08 /* error */
#define TIE 0x04 /* transmit */
#define RIE 0x02 /* recieve */

/*
// Debug macro
*/

#ifdef DEBUG_CANDRV
#define PDEBUG(fmt,arg...) \
          printk(fmt,##arg)
#else
#define PDEBUG(fmt,arg...)
#endif

#endif

/*
// Possible flags in can_msg.flags field
*/

#define MSG_RTR 0x01
#define MSG_OVR 0x02

#endif

























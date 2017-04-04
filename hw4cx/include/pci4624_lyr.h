#ifndef __PCI4624_LYR_H
#define __PCI4624_LYR_H


#include "cxsd_driver.h"


#define PCI4624_LYR_NAME "pci4624"
enum
{
    PCI4624_LYR_VERSION_MAJOR = 2,
    PCI4624_LYR_VERSION_MINOR = 0,
    PCI4624_LYR_VERSION = CX_ENCODE_VERSION(PCI4624_LYR_VERSION_MAJOR,
                                            PCI4624_LYR_VERSION_MINOR)
};


enum
{
    PCI4624_OP_NONE      = 0,
    PCI4624_OP_RD        = 1,
    PCI4624_OP_WR        = 2,

    PCI4624_COND_NONZERO = 1,
    PCI4624_COND_EQUAL   = 0,
    
};


typedef void  (*pci4624_irq_proc)(int devid, void *devptr,
                                  int num_irqs, int got_mask);

typedef void  (*pci4624_Disconnect)(int devid);

typedef int   (*pci4624_Open)(int devid, void *devptr,

                              int pci_vendor_id, int pci_device_id, int pci_instance,
                              int serial_offset,

                              int irq_chk_op,
                              int irq_chk_bar, int irq_chk_ofs, int irq_chk_units,
                              int irq_chk_msk, int irq_chk_val, int irq_chk_cond,

                              int irq_rst_op,
                              int irq_rst_bar, int irq_rst_ofs, int irq_rst_units,
                                               int irq_rst_val,

                              pci4624_irq_proc irq_proc,

                              int on_cls1_op,
                              int on_cls1_bar, int on_cls1_ofs, int on_cls1_units,
                                               int on_cls1_val,
                              int on_cls2_op,
                              int on_cls2_bar, int on_cls2_ofs, int on_cls2_units,
                                               int on_cls2_val
                             );

typedef int32 (*pci4624_ReadBar0) (int handle, int ofs);
typedef void  (*pci4624_WriteBar0)(int handle, int ofs, int32 value);
typedef void  (*pci4624_ReadBar1) (int handle, int ofs, int32 *buf, int count);
typedef void  (*pci4624_WriteBar1)(int handle, int ofs, int32 *buf, int count);

typedef struct
{
    pci4624_Open        Open;

    pci4624_ReadBar0    ReadBar0;
    pci4624_WriteBar0   WriteBar0;
    pci4624_ReadBar1    ReadBar1;
    pci4624_WriteBar1   WriteBar1;
} pci4624_vmt_t;


#endif /* __PCI4624_LYR_H */

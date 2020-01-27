#ifndef __VME_LYR_H
#define __VME_LYR_H


#include "cx_version.h"

#include "cxsd_driver.h"


#define VME_LYR_NAME "vme"
enum
{
    VME_LYR_VERSION_MAJOR = 1,
    VME_LYR_VERSION_MINOR = 0,
    VME_LYR_VERSION = CX_ENCODE_VERSION(VME_LYR_VERSION_MAJOR,
                                        VME_LYR_VERSION_MINOR)
};


/* Driver-provided callbacks */

typedef void (*vme_irq_proc)(int devid, void *devptr,
                             int irq_n, int irq_vect);


/* Layer API for drivers */

typedef int  (*VmeAddDevice) (int devid, void *devptr,
                              uint32 base_addr, uint32 space_size, int am,
                              int irq_n, int irq_vect, vme_irq_proc irq_proc);

typedef int  (*VmeGetDevInfo)(int devid,
                              uint32 *base_addr_p, uint32 *space_size_p, int *am_p,
                              int *irq_n_p, int *irq_vect_p);

typedef int  (*VmeAxxWr8)    (int handle, uint32 ofs, uint8    value);
typedef int  (*VmeAxxRd8)    (int handle, uint32 ofs, uint8   *val_p);
typedef int  (*VmeAxxWr16)   (int handle, uint32 ofs, uint16   value);
typedef int  (*VmeAxxRd16)   (int handle, uint32 ofs, uint16  *val_p);
typedef int  (*VmeAxxWr32)   (int handle, uint32 ofs, uint32   value);
typedef int  (*VmeAxxRd32)   (int handle, uint32 ofs, uint32  *val_p);


typedef struct
{
    VmeAddDevice  add;

    VmeGetDevInfo get_dev_info;

    /* I/O */

    // A16
    VmeAxxWr8     a16wr8;
    VmeAxxRd8     a16rd8;
    VmeAxxWr16    a16wr16;
    VmeAxxRd16    a16rd16;
    VmeAxxWr32    a16wr32;
    VmeAxxRd32    a16rd32;

    // A24
    VmeAxxWr8     a24wr8;
    VmeAxxRd8     a24rd8;
    VmeAxxWr16    a24wr16;
    VmeAxxRd16    a24rd16;
    VmeAxxWr32    a24wr32;
    VmeAxxRd32    a24rd32;

    // A32
    VmeAxxWr8     a32wr8;
    VmeAxxRd8     a32rd8;
    VmeAxxWr16    a32wr16;
    VmeAxxRd16    a32rd16;
    VmeAxxWr32    a32wr32;
    VmeAxxRd32    a32rd32;

} vme_vmt_t;


#endif /* __VME_LYR_H */

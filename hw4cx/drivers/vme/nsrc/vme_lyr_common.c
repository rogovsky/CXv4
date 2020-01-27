#include <stdarg.h>

#include "misc_macros.h"

#include "cxsd_driver.h"
#include "vme_lyr.h"


#ifndef VMEHAL_FILE_H
  #error The "VMEHAL_FILE_H" macro is undefined
#else
  #include VMEHAL_FILE_H
#endif /* VMEHAL_FILE_H */

#ifndef VMELYR_NAME
  #error The "VMELYR_NAME" macro is undefined
#endif /* VMELYR_NAME */


enum
{
    VME_HANDLE_ABSENT = 0,
    VME_HANDLE_FIRST  = 1,
};

enum
{
    MAXDEVS     = 100, // Arbitrary limit, VME crates are usually much smaller
    NUMIRQS     = 8,   // # of IRQs in VME; in fact, IRQ0 is probably non-functional
    VECTSPERIRQ = 256, // Vector is 1 byte
};

typedef struct
{
    int               is_used;

    int               devid;
    void             *devptr;

    uint32            base_addr;
    uint32            space_size; // As of 15.09.2011 -- unused; for future checks in layer
    int               am;

    int               irq_n;
    int               irq_vect;
    vme_irq_proc      irq_proc;
} vmedevinfo_t;

typedef struct
{
    int state;
    int vect2handle[VECTSPERIRQ];
} irqinfo_t;


static int    my_lyrid;

vmedevinfo_t  devs[MAXDEVS];
irqinfo_t     irqs[NUMIRQS];


/*##################################################################*/
/*##################################################################*/

static int   vme_init_lyr  (int lyrid)
{
  int  irq_n;
  int  irq_vect;
  int  r;

    DoDriverLog(my_lyrid, 0, "%s(%d)!", __FUNCTION__, lyrid);
    my_lyrid = lyrid;
    bzero(devs, sizeof(devs));
    bzero(irqs, sizeof(irqs));
    for (irq_n = 0;  irq_n < countof(irqs);  irq_n++)
    {
        irqs[irq_n].state = -1;
        for (irq_vect = 0;  irq_vect < countof(irqs[irq_n].vect2handle);  irq_vect++)
            irqs[irq_n].vect2handle[irq_vect] = VME_HANDLE_ABSENT;
    }

    r = vmehal_init();
    if (r < 0)
    {
        DoDriverLog(my_lyrid, 0, "%s(): vmehal_init()=%d", __FUNCTION__, r);
        return -CXRF_DRV_PROBL;
    }

    return 0;
}

static void  vme_term_lyr  (void)
{
  int  irq_n;

    for (irq_n = 0;  irq_n < countof(irqs);  irq_n++)
    {
        if (irqs[irq_n].state >= 0) vmehal_close_irq(irq_n);
        irqs[irq_n].state = -1;
    }
}

static void  vme_disconnect(int devid)
{
  int           handle;
  int           other_handle;
  int           irq_n;
  int           irq_vect;
  vmedevinfo_t *dp;
  int           was_found = 0;

fprintf(stderr, "%s[%d]\n", __FUNCTION__, devid);
    for (handle = VME_HANDLE_FIRST;  handle < countof(devs);  handle++)
        if (devs[handle].is_used  &&  devs[handle].devid == devid)
        {
            was_found = 1;
            dp = devs + handle;

            irq_n    = dp->irq_n;
            irq_vect = dp->irq_vect;
            bzero(dp, sizeof(&dp));

            // If IRQ was used, then release associated resources
            if (irq_n >= 0)
            {
                // a. Mark this vector as free
                irqs[irq_n].vect2handle[irq_vect] = VME_HANDLE_ABSENT;
                /* b. Check if some other devices still use this IRQ */
                for (other_handle = VME_HANDLE_FIRST;  other_handle < countof(devs);  other_handle++)
                    if (devs[other_handle].is_used  &&
                        devs[other_handle].irq_n == irq_n)
                        goto NEXT_DEV;
                /* ...or was last "client" of this IRQ? */
                /* Then release the IRQ! */
                vmehal_close_irq(irq_n);
                irqs[irq_n].state = -1;
            }

        NEXT_DEV:; //!!! "BREAK_IRQ_CHECK"?
        }

    if (!was_found)
        DoDriverLog(devid, 0, "%s: request to disconnect unknown devid=%d",
                    __FUNCTION__, devid);
}


static void vme_lyr_irq_cb(int irq_n, int irq_vect)
{
  vmedevinfo_t *dp;
  int           handle;

    handle = irqs[irq_n].vect2handle[irq_vect];
fprintf(stderr, "\t%s %d,%d ->%d\n", __FUNCTION__, irq_n, irq_vect, handle);
    if (handle != VME_HANDLE_ABSENT)
    {
        dp = devs + handle;
        if (dp->irq_proc != NULL)
            dp->irq_proc(dp->devid, dp->devptr, irq_n, irq_vect);
    }
}
static int vme_add(int devid, void *devptr,
                   uint32 base_addr, uint32 space_size, int am,
                   int irq_n, int irq_vect, vme_irq_proc irq_proc)
{
  vmedevinfo_t *dp;
  int           handle;
  int           other_devid;

    if (devid == DEVID_NOT_IN_DRIVER)
    {
        DoDriverLog(my_lyrid, 0, "%s: devid==DEVID_NOT_IN_DRIVER", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }

    if (irq_n >= NUMIRQS)
    {
        DoDriverLog(devid, 0, "%s: irq_n=%d, out_of[0,%d]", __FUNCTION__, irq_n, NUMIRQS-1);
        return -CXRF_CFG_PROBL;
    }

    /* Check if requested {irq_n,irq_vect} tuple isn't already used */
    irq_vect &= 0xFF;
    if (irq_n > 0  &&
        irqs[irq_n].vect2handle[irq_vect] != VME_HANDLE_ABSENT)
    {
        other_devid = (irqs[irq_n].vect2handle[irq_vect] > 0  &&
                       irqs[irq_n].vect2handle[irq_vect] < countof(devs))?
            devs[irqs[irq_n].vect2handle[irq_vect]].devid : DEVID_NOT_IN_DRIVER;
        DoDriverLog(devid, 0, "%s: (irq_n=%d,irq_vect=%d) is already in use (handle=%d, devid=%d)",
                    __FUNCTION__, irq_n, irq_vect,
                    irqs[irq_n].vect2handle[irq_vect],
                    other_devid);
        if (other_devid != DEVID_NOT_IN_DRIVER)
            ReturnDataSet(other_devid, RETURNDATA_COUNT_PONG,
                          NULL, NULL, NULL, NULL, NULL, NULL);
        return -CXRF_CFG_PROBL;
    }

    /* Find a free cell... */
    for (handle = VME_HANDLE_FIRST;  handle < countof(devs);  handle++)
        if (devs[handle].is_used == 0) break;
    if (handle >= countof(devs))
    {
        DoDriverLog(devid, 0, "%s: devs[] overflow (too many devices requested)", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }
    /* ...and mark it as used */
    dp = devs + handle;
    dp->is_used = 1;

    /* Remember info */
    dp->devid      = devid;
    dp->devptr     = devptr;
    dp->base_addr  = base_addr;
    dp->space_size = space_size;
    dp->am         = am;
    dp->irq_n      = irq_n;
    dp->irq_vect   = irq_vect;
    dp->irq_proc   = irq_proc;

    if (irq_n > 0)
    {
        if (irqs[irq_n].state < 0)
        {
            irqs[irq_n].state = vmehal_open_irq(irq_n, vme_lyr_irq_cb, my_lyrid);
            if (irqs[irq_n].state < 0)
            {
                bzero(dp, sizeof(*dp));
                return -CXRF_DRV_PROBL;
            }
        }
        irqs[irq_n].vect2handle[irq_vect] = handle;
    }

    //!!!!!!!!!!! Get an irq!

    return handle;
}

static int vme_get_dev_info(int devid,
                            uint32 *base_addr_p, uint32 *space_size_p, int *am_p,
                            int *irq_n_p, int *irq_vect_p)
{
  int           handle;
  vmedevinfo_t *dp;

    for (handle = VME_HANDLE_FIRST;  handle < countof(devs);  handle++)
        if (devs[handle].is_used  &&  devs[handle].devid == devid)
        {
            dp = devs + handle;
            if (base_addr_p  != NULL) *base_addr_p  = dp->base_addr;
            if (space_size_p != NULL) *space_size_p = dp->space_size;
            if (am_p         != NULL) *am_p         = dp->am;
            if (irq_n_p      != NULL) *irq_n_p      = dp->irq_n;
            if (irq_vect_p   != NULL) *irq_vect_p   = dp->irq_vect;

            return 0;
        }


    return -1;
}

#define VMELYR_DEFINE_IO(AS, TS)                                      \
static int vme_a##AS##wr##TS(int handle, uint32 ofs, uint##TS  value) \
{                                                                     \
    return vmehal_a##AS##wr##TS(devs[handle].am,                      \
                                devs[handle].base_addr + ofs, value); \
}                                                                     \
static int vme_a##AS##rd##TS(int handle, uint32 ofs, uint##TS *val_p) \
{                                                                     \
    return vmehal_a##AS##rd##TS(devs[handle].am,                      \
                                devs[handle].base_addr + ofs, val_p); \
}

VMELYR_DEFINE_IO(16, 8)
VMELYR_DEFINE_IO(16, 16)
VMELYR_DEFINE_IO(16, 32)

VMELYR_DEFINE_IO(24, 8)
VMELYR_DEFINE_IO(24, 16)
VMELYR_DEFINE_IO(24, 32)

VMELYR_DEFINE_IO(32, 8)
VMELYR_DEFINE_IO(32, 16)
VMELYR_DEFINE_IO(32, 32)


static vme_vmt_t vme_vmt =
{
    vme_add,

    vme_get_dev_info,

    // A16
    vme_a16wr8,
    vme_a16rd8,
    vme_a16wr16,
    vme_a16rd16,
    vme_a16wr32,
    vme_a16rd32,

    // A24
    vme_a24wr8,
    vme_a24rd8,
    vme_a24wr16,
    vme_a24rd16,
    vme_a24wr32,
    vme_a24rd32,

    // A32
    vme_a32wr8,
    vme_a32rd8,
    vme_a32wr16,
    vme_a32rd16,
    vme_a32wr32,
    vme_a32rd32,
};

DEFINE_CXSD_LAYER(VMELYR_NAME, "VME-layer implementation via '" __CX_STRINGIZE(VMEHAL_DESCR) "' HAL",
                  NULL, NULL,
                  VME_LYR_NAME, VME_LYR_VERSION,
                  vme_init_lyr, vme_term_lyr,
                  vme_disconnect,
                  &vme_vmt);

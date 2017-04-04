#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "misc_macros.h"

#include "cxsd_driver.h"

#include "uspci.h"

#include "pci4624_lyr.h"


enum
{
    MAXDEVS = 100, // An arbitrary limit
};

typedef struct
{
    int               fd;
    sl_fdh_t          fdhandle;
    int               devid;
    void             *devptr;
    pci4624_irq_proc  irq_proc;
} pci4624_devinfo_t;


static int               my_lyrid;

static pci4624_devinfo_t devs[MAXDEVS];


static int   pci4624_init_lyr  (int lyrid)
{
  int         handle;

    my_lyrid = lyrid;

    bzero(&devs, sizeof(devs));
    for (handle = 0;  handle < countof(devs);  handle++)
        devs[handle].fd = -1;

    return 0;
}

static void  pci4624_term_lyr  (void)
{
    /*!!!*/
}

static void  pci4624_disconnect(int devid)
{
  int  handle;

////fprintf(stderr, "%s(%d)\n", __FUNCTION__, devid);
    for (handle = 0;
         handle < countof(devs);
         handle++)
        if (devs[handle].fd >= 0  &&  devs[handle].devid == devid)
        {
            sl_del_fd(devs[handle].fdhandle);
            close(devs[handle].fd);
            devs[handle].fd = -1;
        }
}

static void pci4624_irq_fd_p(int devid, void *devptr,
                             sl_fdh_t fdhandle, int fd, int mask,
                             void *privptr)
{
  int    handle = ptr2lint(privptr);
  int    num_irqs;
  __u32  got_mask;

    ////DoDriverLog(-1, 0, __FUNCTION__);
    num_irqs = ioctl(devs[handle].fd, USPCI_FGT_IRQ, &got_mask);
    if (devs[handle].irq_proc != NULL)
        devs[handle].irq_proc(devid, devs[handle].devptr,
                              num_irqs, got_mask);
}

static void PrintAddr(const char *descr, uspci_rw_params_t *addr_p)
{
    printf("%sbar=%d ofs=%d u=%d count=%d addr=%p cond=%d mask=%u value=%u t_op=%d\n",
           descr,
           addr_p->bar, addr_p->offset, addr_p->units, addr_p->count,
           addr_p->addr, addr_p->cond, addr_p->mask, addr_p->value, addr_p->t_op);
}

static int  pci4624_open(int devid, void *devptr,
                           
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
                        )
{
  int                      handle;
  pci4624_devinfo_t       *dp;

  uspci_setpciid_params_t  uspci_dev;
  uspci_irq_params_t       uspci_irq;
  uspci_rw_params_t        onclose_params[2];
  int                      onclose_count;

    if (pci_instance < 0  &&  0)
    {
        DoDriverLog(devid, 0, "addressing by serial-ID (ID=%d) int't supported yet",
                    -pci_instance);
        return -CXRF_CFG_PROBL;
    }

    /* Find a free slot */
    for (handle = 0;
         handle < countof(devs)  &&  devs[handle].fd >= 0;
         handle++);
    if (handle >= countof(devs))
    {
        errno = E2BIG;
        return -CXRF_DRV_PROBL;
    }
    dp = devs + handle;

    /* Remember info */
    dp->devid    = devid;
    dp->devptr   = devptr;
    dp->irq_proc = irq_proc;

    /* Open /dev/uspci */
    if ((dp->fd = open("/dev/uspci", O_RDONLY)) < 0)
    {
        DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "open(\"/dev/uspci\")");
        goto CLEANUP_ON_ERROR;
    }

    /* Set the device */
    bzero(&uspci_dev, sizeof(uspci_dev));
    if (pci_instance >= 0)
    {
        uspci_dev.t_addr    = USPCI_SETDEV_BY_NUM;
        uspci_dev.vendor_id = pci_vendor_id;
        uspci_dev.device_id = pci_device_id;
        uspci_dev.n         = pci_instance;
    }
    else
    {
        uspci_dev.t_addr    = USPCI_SETDEV_BY_SERIAL;
        uspci_dev.vendor_id = pci_vendor_id;
        uspci_dev.device_id = pci_device_id;
        uspci_dev.serial.bar    = 0;
        uspci_dev.serial.offset = serial_offset;
        uspci_dev.serial.units  = sizeof(int32);
        uspci_dev.serial.value  = -pci_instance;
    }
    if (ioctl(dp->fd, USPCI_SETPCIID, &uspci_dev) < 0)
    {
        DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "USPCI_SETPCIID");
        goto CLEANUP_ON_ERROR;
    }

    /* Specify what to do upon close */
    bzero(&onclose_params, sizeof(onclose_params));
    onclose_count = 0;
    if (on_cls1_op != PCI4624_OP_NONE)
    {
        onclose_params[onclose_count].t_op   = on_cls1_op == PCI4624_OP_RD? USPCI_OP_READ
                                                                          : USPCI_OP_WRITE;
        onclose_params[onclose_count].bar    = on_cls1_bar;
        onclose_params[onclose_count].offset = on_cls1_ofs;
        onclose_params[onclose_count].units  = on_cls1_units;
        onclose_params[onclose_count].value  = on_cls1_val;

        onclose_count++;
    }
    if (on_cls2_op != PCI4624_OP_NONE)
    {
        onclose_params[onclose_count].t_op   = on_cls2_op == PCI4624_OP_RD? USPCI_OP_READ
                                                                          : USPCI_OP_WRITE;
        onclose_params[onclose_count].bar    = on_cls2_bar;
        onclose_params[onclose_count].offset = on_cls2_ofs;
        onclose_params[onclose_count].units  = on_cls2_units;
        onclose_params[onclose_count].value  = on_cls2_val;

        onclose_count++;
    }
    if (onclose_count != 0)
    {
        onclose_params[0].batch_count = onclose_count;
        if (ioctl(dp->fd, USPCI_ON_CLOSE, onclose_params) < 0  &&  0)
        {
            DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "USPCI_ON_CLOSE");
            goto CLEANUP_ON_ERROR;
        }
    }
    
    /* Request IRQ handling, if specified */
    if (irq_chk_op != PCI4624_OP_NONE)
    {
        bzero(&uspci_irq, sizeof(uspci_irq));

        uspci_irq.check.t_op   = USPCI_OP_READ;
        uspci_irq.check.bar    = irq_chk_bar;
        uspci_irq.check.offset = irq_chk_ofs;
        uspci_irq.check.units  = irq_chk_units;
        uspci_irq.check.mask   = irq_chk_msk;
        uspci_irq.check.value  = irq_chk_val;
        uspci_irq.check.count  = 1;
        uspci_irq.check.cond   = irq_chk_cond == PCI4624_COND_NONZERO? USPCI_COND_NONZERO
                                                                     : USPCI_COND_EQUAL;
        if (irq_rst_op != PCI4624_OP_NONE)
        {
            uspci_irq.reset.t_op   = irq_rst_op == PCI4624_OP_RD? USPCI_OP_READ
                                                                : USPCI_OP_WRITE;
            uspci_irq.reset.bar    = irq_rst_bar;
            uspci_irq.reset.offset = irq_rst_ofs;
            uspci_irq.reset.units  = irq_rst_units;
            uspci_irq.reset.value  = irq_rst_val;
        }

        if (ioctl(dp->fd, USPCI_SET_IRQ, &uspci_irq) < 0)
        {
            DoDriverLog(devid, 0 | DRIVERLOG_ERRNO, "USPCI_SET_IRQ");
            goto CLEANUP_ON_ERROR;
        }

        ////PrintAddr("check", &uspci_irq.check);
        ////PrintAddr("reset", &uspci_irq.reset);
        
        dp->fdhandle = sl_add_fd(devid, devptr,
                                 dp->fd, SL_EX,
                                 pci4624_irq_fd_p, lint2ptr(handle));
    }

    return handle;

CLEANUP_ON_ERROR:
    if (dp->fd > 0) close(dp->fd);
    dp->fd = -1;
    return -CXRF_DRV_PROBL;
}


#define DECODE_AND_CHECK(errret)                                 \
    do {                                                         \
        if (handle < 0  ||  handle >= countof(devs))             \
        {                                                        \
            DoDriverLog(my_lyrid, 0,                             \
                        "%s: invalid handle %d",                 \
                        __FUNCTION__, handle);                   \
            return errret;                                       \
        }                                                        \
        if (devs[handle].fd < 0)                                 \
        {                                                        \
            DoDriverLog(my_lyrid, 0,                             \
                        "%s: attempt to use free handle %d",     \
                        __FUNCTION__, handle);                   \
            return errret;                                       \
        }                                                        \
        dp = devs + handle;                                      \
    } while (0)

static int32 pci4624_readbar0 (int handle, int ofs)
{
  pci4624_devinfo_t       *dp;
  uspci_rw_params_t        iop;
  int32                    ret;

    DECODE_AND_CHECK(-1);

    bzero(&iop, sizeof(iop));
    iop.bar    = 0;
    iop.offset = ofs;
    iop.units  = sizeof(ret);
    iop.count  = 1;
    iop.addr   = &ret;
    iop.t_op   = USPCI_OP_READ;

    ioctl(dp->fd, USPCI_DO_IO, &iop);

    return ret;
}

static void  pci4624_writebar0(int handle, int ofs, int32 value)
{
  pci4624_devinfo_t       *dp;
  uspci_rw_params_t        iop;

//raise(11);
    DECODE_AND_CHECK();
    
    bzero(&iop, sizeof(iop));
    iop.bar    = 0;
    iop.offset = ofs;
    iop.units  = sizeof(value);
    iop.count  = 1;
    iop.addr   = &value;
    iop.t_op   = USPCI_OP_WRITE;

    ioctl(dp->fd, USPCI_DO_IO, &iop);
}

static void  pci4624_readbar1 (int handle, int ofs, int32 *buf, int count)
{
  pci4624_devinfo_t       *dp;
  uspci_rw_params_t        iop;

    DECODE_AND_CHECK();

    bzero(&iop, sizeof(iop));
    iop.bar    = 1;
    iop.offset = ofs;
    iop.units  = sizeof(*buf);
    iop.count  = count;
    iop.addr   = buf;
    iop.t_op   = USPCI_OP_READ;
    
    ioctl(dp->fd, USPCI_DO_IO, &iop);
}

static void  pci4624_writebar1(int handle, int ofs, int32 *buf, int count)
{
  pci4624_devinfo_t       *dp;
  uspci_rw_params_t        iop;

    DECODE_AND_CHECK();

    bzero(&iop, sizeof(iop));
    iop.bar    = 1;
    iop.offset = ofs;
    iop.units  = sizeof(*buf);
    iop.count  = count;
    iop.addr   = buf;
    iop.t_op   = USPCI_OP_WRITE;
    
    ioctl(dp->fd, USPCI_DO_IO, &iop);
}

pci4624_vmt_t pci4624_vmt =
{
    pci4624_open,
    pci4624_readbar0,
    pci4624_writebar0,
    pci4624_readbar1,
    pci4624_writebar1,
};

DEFINE_CXSD_LAYER(pci4624, "pci4624 access",
                  NULL, NULL,
                  PCI4624_LYR_NAME, PCI4624_LYR_VERSION,
                  pci4624_init_lyr, pci4624_term_lyr,
                  pci4624_disconnect,
                  &pci4624_vmt);

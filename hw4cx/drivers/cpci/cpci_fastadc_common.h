#include "misc_macros.h"
#include "pzframe_drv.h"
#include "pci4624_lyr.h"


#ifndef FASTADC_NAME
  #error The "FASTADC_NAME" is undefined
#endif


#define FASTADC_PRIVREC_T __CX_CONCATENATE(FASTADC_NAME,_privrec_t)
#define FASTADC_PARAMS    __CX_CONCATENATE(FASTADC_NAME,_params)
#define FASTADC_INIT_D    __CX_CONCATENATE(FASTADC_NAME,_init_d)
#define FASTADC_TERM_D    __CX_CONCATENATE(FASTADC_NAME,_term_d)
#define FASTADC_RW_P      __CX_CONCATENATE(FASTADC_NAME,_rw_p)
#define FASTADC_IRQ_P     __CX_CONCATENATE(FASTADC_NAME,_irq_p)


static void FASTADC_IRQ_P (int devid, void *devptr,
                           int num_irqs __attribute__((unused)),
                           int got_mask __attribute__((unused)))
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;

    ////printf("IRQ!!! mn=%d\n", me->pz.measuring_now);
    if (me->do_return == 0  &&  me->force_read) ReadMeasurements(&(me->pz));
    pzframe_drv_drdy_p(&(me->pz), me->do_return, 0);
}

static int  FASTADC_INIT_D(int devid, void *devptr, 
                           int businfocount, int businfo[],
                           const char *auxinfo __attribute__((unused)))
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;
  int                 n;

  enum {BAR0 = 0};

    me->devid  = devid;
    me->lvmt   = GetLayerVMT(devid);
    me->handle = me->lvmt->Open(devid, me,
                                FASTADC_VENDOR_ID, FASTADC_DEVICE_ID, businfo[0],
                                FASTADC_SERIAL_OFFSET,

                                FASTADC_IRQ_CHK_OP,
                                BAR0, FASTADC_IRQ_CHK_OFS, sizeof(int32),
                                FASTADC_IRQ_CHK_MSK, FASTADC_IRQ_CHK_VAL, FASTADC_IRQ_CHK_COND,

                                FASTADC_IRQ_RST_OP,
                                BAR0, FASTADC_IRQ_RST_OFS, sizeof(int32),
                                FASTADC_IRQ_RST_VAL,

                                FASTADC_IRQ_P,

                                FASTADC_ON_CLS1_OP,
                                BAR0, FASTADC_ON_CLS1_OFS, sizeof(int32),
                                FASTADC_ON_CLS1_VAL,

                                FASTADC_ON_CLS2_OP,
                                BAR0, FASTADC_ON_CLS2_OFS, sizeof(int32),
                                FASTADC_ON_CLS2_VAL);
    if (me->handle < 0) return me->handle;

    for (n = 0;  n < countof(chinfo);  n++)
        if (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
            chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);

    pzframe_drv_init(&(me->pz), devid,
                     PARAM_SHOT, PARAM_ISTART, PARAM_WAITTIME, PARAM_STOP, PARAM_ELAPSED,
                     StartMeasurements, TrggrMeasurements,
                     AbortMeasurements, ReadMeasurements,
                     PrepareRetbufs);

    return InitParams(&(me->pz));
}

static void FASTADC_TERM_D(int devid, void *devptr)
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;

    if (me->handle < 0) return; // For non-initialized devices

    if      (FASTADC_ON_CLS1_OP == PCI4624_OP_RD)
        me->lvmt->ReadBar0(me->handle,  FASTADC_ON_CLS1_OFS);
    else if (FASTADC_ON_CLS1_OP == PCI4624_OP_WR)
        me->lvmt->WriteBar0(me->handle, FASTADC_ON_CLS1_OFS, FASTADC_ON_CLS1_VAL);

    if      (FASTADC_ON_CLS2_OP == PCI4624_OP_RD)
        me->lvmt->ReadBar0(me->handle,  FASTADC_ON_CLS2_OFS);
    else if (FASTADC_ON_CLS2_OP == PCI4624_OP_WR)
        me->lvmt->WriteBar0(me->handle, FASTADC_ON_CLS2_OFS, FASTADC_ON_CLS2_VAL);

    pzframe_drv_term(&(me->pz));
}


DEFINE_CXSD_DRIVER(FASTADC_NAME, __CX_STRINGIZE(FASTADC_NAME) " fast-ADC",
                   NULL, NULL,
                   sizeof(FASTADC_PRIVREC_T), FASTADC_PARAMS,
                   1, 1,
                   PCI4624_LYR_NAME, PCI4624_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   FASTADC_INIT_D, FASTADC_TERM_D, FASTADC_RW_P);

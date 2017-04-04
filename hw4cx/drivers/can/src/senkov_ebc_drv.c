#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/senkov_ebc_drv_i.h"


enum
{
    DEVTYPE = 0x20,
};

enum
{
    DESC_GETMES        = 0x02,
    DESC_WR_DAC_n_base = 0x80,
    DESC_RD_DAC_n_base = 0x90,
};


/*  */

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;
    
} privrec_t;

static psp_paramdescr_t senkov_ebc_params[] =
{
    PSP_P_END()
};



static void senkov_ebc_in(int devid, void *devptr,
                          int desc, size_t dlc, uint8 *data);

static int senkov_ebc_init_d(int devid, void *devptr, 
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t *me    = (privrec_t *) devptr;
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "ENTRY %s(%s)", __FUNCTION__, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               NULL, senkov_ebc_in,
                               SENKOV_EBC_NUMCHANS * 2);
    if (me->handle < 0) return me->handle;
    me->lvmt->has_regs(me->handle, SENKOV_EBC_CHAN_REGS_base);

    SetChanRDs(devid, SENKOV_EBC_CHAN_DAC_n_base + 0, 1, 5000/100.0, 0.0);
    SetChanRDs(devid, SENKOV_EBC_CHAN_ADC_n_base + 0, 1, 5000/50.0,  0.0);
    SetChanRDs(devid, SENKOV_EBC_CHAN_ADC_n_base + 1, 1, 5000/100.0, 0.0);

    return DEVSTATE_OPERATING;
}

static void senkov_ebc_in(int devid, void *devptr __attribute__((unused)),
                          int desc, size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         attr;
  int32       val;

    switch (desc)
    {
        case DESC_GETMES:
            me->lvmt->q_erase_and_send_next_v(me->handle, 2, desc, data[1]);
            if (dlc < 4) return;
            
            attr   = data[1];
            val    = data[2] + data[3] * 256;
            if (attr < SENKOV_EBC_CHAN_ADC_n_count)
                ReturnInt32Datum(devid, SENKOV_EBC_CHAN_ADC_n_base + attr,
                                 val, 0);
            else
                DoDriverLog(devid, 0, "in: GETMES attr=%d, >%d (val=%d)",
                            attr, SENKOV_EBC_CHAN_ADC_n_count-1, val);
            break;

        case DESC_RD_DAC_n_base ... DESC_RD_DAC_n_base + SENKOV_EBC_CHAN_DAC_n_count-1:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 3) return;
            
            attr   = desc - DESC_RD_DAC_n_base;
            val    = data[1] + data[2] * 256;
            ReturnInt32Datum(devid, SENKOV_EBC_CHAN_DAC_n_base + attr, val, 0);
            break;
    }
}

static void senkov_ebc_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int         chn;   // channel
  int32       val;   // Value
  int         attr;
  int         code;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        if      (chn >= SENKOV_EBC_CHAN_REGS_base  &&  chn <= SENKOV_EBC_CHAN_REGS_last)
            me->lvmt->regs_rw(me->handle, action, chn, &val);
        else if (chn >= SENKOV_EBC_CHAN_ADC_n_base  &&
                 chn <= SENKOV_EBC_CHAN_ADC_n_base + SENKOV_EBC_CHAN_ADC_n_count-1)
        {
            /* ADC -- always read, don't care about "action" */
            attr = chn - SENKOV_EBC_CHAN_ADC_n_base;
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 2,
                              DESC_GETMES, attr);
        }
        else if (chn >= SENKOV_EBC_CHAN_DAC_n_base  &&
                 chn <= SENKOV_EBC_CHAN_DAC_n_base + SENKOV_EBC_CHAN_DAC_n_count-1)
        {
            if (chn > SENKOV_EBC_CHAN_DAC_n_base) continue; // This weird check is because Senkov doesn't reply to other channels (>0)
            attr = chn - SENKOV_EBC_CHAN_DAC_n_base;
            if (action == DRVA_WRITE)
            {
                code = val;
                if (code < 0)    code = 0;
                if (code > 5000) code = 5000;

                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 3,
                                  DESC_WR_DAC_n_base + attr,
                                  code & 0xFF, (code >> 8) & 0xFF);
            }
            
            /* Perform read anyway */
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                              DESC_RD_DAC_n_base + attr);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

/* Metric */

DEFINE_CXSD_DRIVER(senkov_ebc, "Senkov/Pureskin EBC (Electron-Beam Controller)",
                   NULL, NULL,
                   sizeof(privrec_t), senkov_ebc_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   senkov_ebc_init_d, NULL, senkov_ebc_rw_p);

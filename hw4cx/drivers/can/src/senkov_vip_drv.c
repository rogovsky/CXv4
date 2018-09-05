#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/senkov_vip_drv_i.h"


enum
{
    DEVTYPE = 0x30*0+-1,
};

enum
{
    DESC_GETMES   = 0x04,
    DESC_WR_DAC   = 0x05,
    DESC_RD_DAC   = 0x06,
    DESC_READSTAT = CANKOZ_DESC_READREGS,
    DESC_WRITECTL = CANKOZ_DESC_WRITEOUTREG,
};

static int max_dac_values[SENKOV_VIP_SET_n_count] =
{
    0,
    700,
    255,
    255,
    255,
    1000,
    12000,
    20
};

enum {HEARTBEAT_USECS = 500*1000};


/*  */

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

} privrec_t;

static psp_paramdescr_t senkov_vip_params[] =
{
    PSP_P_END()
};


static void senkov_vip_in (int devid, void *devptr,
                           int desc, size_t dlc, uint8 *data);
static void senkov_vip_hbt(int devid, void *devptr,
                           sl_tid_t tid,
                           void *privptr);

static int senkov_vip_init_d(int devid, void *devptr, 
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
                               NULL, senkov_vip_in,
                               SENKOV_VIP_NUMCHANS * 2,
                               CANKOZ_LYR_OPTION_NONE);
    if (me->handle < 0) return me->handle;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, senkov_vip_hbt, NULL);

    SetChanRDs(devid, SENKOV_VIP_CHAN_SET_DAC_UOUT,      1,  10,  0.0);
    SetChanRDs(devid, SENKOV_VIP_CHAN_SET_VIP_IOUT_PROT, 1,  10,  0.0);
    SetChanRDs(devid, SENKOV_VIP_CHAN_SET_BRKDN_COUNTER, 1,   1, -1.0);
    SetChanRDs(devid, SENKOV_VIP_CHAN_MES_VIP_UOUT,      1, 100,  0.0);
    SetChanRDs(devid, SENKOV_VIP_CHAN_MES_VIP_IOUT_FULL, 1,  10,  0.0);
    SetChanRDs(devid, SENKOV_VIP_CHAN_MES_VIP_IOUT_AUTO, 1,  10,  0.0);
    SetChanRDs(devid, SENKOV_VIP_CHAN_MES_VIP_IOUT_AVG,  1,  10,  0.0);

    return DEVSTATE_OPERATING;
}


static void senkov_vip_in(int devid, void *devptr __attribute__((unused)),
                          int desc, size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         attr;
  int         code;
  int         b;
  int32       val;

  enum {GRPSIZE = (SENKOV_VIP_MODE_sh_count > SENKOV_VIP_STAT_sh_count)?
                   SENKOV_VIP_MODE_sh_count : SENKOV_VIP_STAT_sh_count};
  int                grpaddrs [GRPSIZE];
  int32              grpvals  [GRPSIZE];
  void              *grpvals_p[GRPSIZE];

  static cxdtype_t   grpdtypes[GRPSIZE] = {[0 ... GRPSIZE-1] = CXDTYPE_INT32};
  static int         grpnelems[GRPSIZE] = {[0 ... GRPSIZE-1] = 1};
  static rflags_t    grprflags[GRPSIZE] = {[0 ... GRPSIZE-1] = 0};

    switch (desc)
    {
        case DESC_GETMES:
            attr = data[1];
            me->lvmt->q_erase_and_send_next_v(me->handle, 2, desc, attr);
            if (dlc < 4) return;
            
            if (attr < SENKOV_VIP_MES_n_count)
            {
                val    = data[2] + data[3] * 256;
                ReturnInt32Datum(devid, SENKOV_VIP_CHAN_MES_base + attr,
                                 val, 0);
            }
            else
            {
                DoDriverLog(devid, 0, "%s(): DESC_GETMES attr=%d, >max=%d",
                            __FUNCTION__, attr, SENKOV_VIP_MES_n_count-1);
            }
            break;

        case DESC_RD_DAC:
            attr = data[1];
            me->lvmt->q_erase_and_send_next_v(me->handle, 2, desc, attr);
            if (dlc < 4) return;
            
            if (attr < SENKOV_VIP_SET_n_count)
            {
                val    = data[2] + data[3] * 256;
                ReturnInt32Datum(devid, SENKOV_VIP_CHAN_SET_base + attr,
                                 val, 0);
            }
            else
            {
                DoDriverLog(devid, 0, "%s(): DESC_RD_DAC attr=%d, >max=%d",
                            __FUNCTION__, attr, SENKOV_VIP_SET_n_count-1);
            }
            break;

        case DESC_READSTAT:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 5) return;

            // Mode
            code = data[1];
            for (b = 0;  b < SENKOV_VIP_MODE_sh_count;  b++)
            {
                grpaddrs [b] = SENKOV_VIP_CHAN_MODE_base + b;
                grpvals  [b] = code & 1;
                grpvals_p[b] = grpvals + b;
                code >>= 1;
            }
            ReturnDataSet(devid,
                          SENKOV_VIP_MODE_sh_count,
                          grpaddrs,  grpdtypes, grpnelems,
                          grpvals_p, grprflags, NULL);

            // Status
            code = data[2] + data[3] * 256;
            for (b = 0;  b < SENKOV_VIP_STAT_sh_count;  b++)
            {
                grpaddrs [b] = SENKOV_VIP_CHAN_STAT_base + b;
                grpvals  [b] = code & 1;
                grpvals_p[b] = grpvals + b;
                code >>= 1;
            }
            ReturnDataSet(devid,
                          SENKOV_VIP_STAT_sh_count,
                          grpaddrs,  grpdtypes, grpnelems,
                          grpvals_p, grprflags, NULL);
            
            // Count
            val    = data[4];
            ReturnInt32Datum(devid, SENKOV_VIP_CHAN_CUR_BRKDN_COUNTER, val, 0);
            break;
    }
}

static void senkov_vip_hbt(int devid, void *devptr,
                           sl_tid_t tid,
                           void *privptr)
{
  privrec_t  *me    = (privrec_t *) devptr;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, senkov_vip_hbt, NULL);

    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 2, 
                      DESC_GETMES, SENKOV_VIP_MES_n_VIP_UOUT);  
}

static void senkov_vip_rw_p(int devid, void *devptr,
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

        if      (chn >= SENKOV_VIP_CHAN_SET_base  &&
                 chn <= SENKOV_VIP_CHAN_SET_base + SENKOV_VIP_SET_n_count-1  &&
                 chn != SENKOV_VIP_CHAN_SET_DAC_RESERVED0)
        {
            attr = chn - SENKOV_VIP_CHAN_SET_base;
            if (action == DRVA_WRITE)
            {
                code = val;
                if (code < 0)                    code = 0;
                if (code > max_dac_values[attr]) code = max_dac_values[attr];

                me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                                      SQ_TRIES_DIR, 0,
                                      NULL, NULL,
                                      0, 4,
                                      DESC_WR_DAC, attr,
                                      code & 0xFF, (code >> 8) & 0xFF);
            }

            /* Perform read anyway */
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 2,
                              DESC_RD_DAC, attr);
        }
        else if (chn >= SENKOV_VIP_CHAN_CMD_base  &&
                 chn <= SENKOV_VIP_CHAN_CMD_base + SENKOV_VIP_CMD_sh_count-1)
        {
            attr = chn - SENKOV_VIP_CHAN_CMD_base;
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                                      SQ_TRIES_DIR, 0,
                                      NULL, NULL,
                                      0, 2,
                                      DESC_WRITECTL, 1 << attr);

            // Return 0 for button-type channel
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn >= SENKOV_VIP_CHAN_MES_base  &&
                 chn <= SENKOV_VIP_CHAN_MES_base + SENKOV_VIP_MES_n_count-1  &&
                 chn != SENKOV_VIP_CHAN_MES_RESERVED0)
        {
            attr = chn - SENKOV_VIP_CHAN_MES_base;
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 2,
                              DESC_GETMES, attr);
        }
        else if ((chn >= SENKOV_VIP_CHAN_MODE_base  &&
                  chn <= SENKOV_VIP_CHAN_MODE_base + SENKOV_VIP_MODE_sh_count-1)
                 ||
                 (chn >= SENKOV_VIP_CHAN_STAT_base  &&
                  chn <= SENKOV_VIP_CHAN_STAT_base + SENKOV_VIP_STAT_sh_count-1)
                 ||
                 chn == SENKOV_VIP_CHAN_CUR_BRKDN_COUNTER)
        {
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                              DESC_READSTAT);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

/* Metric */

DEFINE_CXSD_DRIVER(senkov_vip, "Senkov VIP (high-voltage power supply)",
                   NULL, NULL,
                   sizeof(privrec_t), senkov_vip_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   senkov_vip_init_d, NULL, senkov_vip_rw_p);

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/canadc40_drv_i.h"


/*=== CANADC40 specifics ===========================================*/

enum
{
    DEVTYPE   = 2, /* CANADC40 is 2 */
};

enum
{
    DESC_STOP        = 0x00,
    DESC_MULTICHAN   = 0x01,
    DESC_OSCILL      = 0x02,
    DESC_READONECHAN = 0x03,
    DESC_READRING    = 0x04,

    MODE_BIT_LOOP = 1 << 4,
    MODE_BIT_SEND = 1 << 5,
};

enum
{
    MIN_TIMECODE = 0,
    MAX_TIMECODE = 7,
    DEF_TIMECODE = 4,

    MIN_GAIN     = 0,
    MAX_GAIN     = 1,
    DEF_GAIN     = 0,
};

enum
{
    ALIVE_SECONDS   = 60,
    ALIVE_USECS     = ALIVE_SECONDS * 1000000,
};


static inline uint8 PackMode(int gain_evn, int gain_odd)
{
    return gain_evn | (gain_odd << 2) | MODE_BIT_LOOP | MODE_BIT_SEND;
}

static inline void  decode_attr(uint8 attr, int *chan_p, int *gain_code_p)
{
    *chan_p      = attr & 63;
    *gain_code_p = (attr >> 6) & 3;
}

static rflags_t kozadc_decode_le24(uint8 *bytes,  int    gain_code,
                                   int32 *code_p, int32 *val_p)
{
  int32  code;

  static int ranges[4] =
        {
            10000000 / 1,
            10000000 / 10,
            10000000 / 100,
            10000000 / 1000
        };

    code = bytes[0] | ((uint32)(bytes[1]) << 8) | ((uint32)(bytes[2]) << 16);
    if ((code &  0x800000) != 0)
        code -= 0x1000000;
    
    *code_p = code;
    *val_p  = scale32via64(code, ranges[gain_code], 0x3fffff);
    
    return (code >= -0x3FFFFF && code <= 0x3FFFFF)? 0
                                                  : CXRF_OVERLOAD;

}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int              rd_rcvd;
    
    int              ch_beg;
    int              ch_end;
    int              timecode;
    int              gain;
    int              fast;

    int32            ch_cfg[KOZDEV_CONFIG_CHAN_count];
} privrec_t;

static psp_paramdescr_t canadc40_params[] =
{
    PSP_P_INT ("beg",  privrec_t, ch_beg,   0,                           0, CANADC40_CHAN_ADC_n_count-1),
    PSP_P_INT ("end",  privrec_t, ch_end,   CANADC40_CHAN_ADC_n_count-1, 0, CANADC40_CHAN_ADC_n_count-1),
    PSP_P_INT ("adc_timecode", privrec_t, timecode, DEF_TIMECODE,              MIN_TIMECODE, MAX_TIMECODE),
    PSP_P_INT ("gain", privrec_t, gain,     DEF_GAIN,                    MIN_GAIN,     MAX_GAIN),
    PSP_P_FLAG("fast", privrec_t, fast,     1, 0),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

enum
{
    DSRF_RESET_CHANS = 1 << 0,
    DSRF_RETCONFIGS  = 1 << 1,
};

static int8 SUPPORTED_chans[KOZDEV_CONFIG_CHAN_count] =
{
    [KOZDEV_CHAN_DO_RESET]     = 1,
    [KOZDEV_CHAN_ADC_MODE]     = 1,
    [KOZDEV_CHAN_ADC_BEG]      = 1,
    [KOZDEV_CHAN_ADC_END]      = 1,
    [KOZDEV_CHAN_ADC_TIMECODE] = 1,
    [KOZDEV_CHAN_ADC_GAIN]     = 1,
    [KOZDEV_CHAN_HW_VER]       = 1,
    [KOZDEV_CHAN_SW_VER]       = 1,
};

#define ADC_MODE_IS_NORM() \
    (me->ch_cfg[KOZDEV_CHAN_ADC_MODE-KOZDEV_CONFIG_CHAN_base] == KOZDEV_ADC_MODE_NORM)

static void ReturnCtlCh(privrec_t *me, int chan)
{
    ReturnInt32Datum(me->devid, chan, me->ch_cfg[chan],
                     SUPPORTED_chans[chan]? 0 : CXRF_UNSUPPORTED);
}

static void SetTmode(privrec_t *me, int mode)
{
}

static void SendMULTICHAN(privrec_t *me)
{
  int   dlc;
  uint8 data[8];

    if (!me->fast  ||
        me->ch_cfg[KOZDEV_CHAN_ADC_BEG] != me->ch_cfg[KOZDEV_CHAN_ADC_END])
    {
        dlc     = 6;
        data[0] = DESC_MULTICHAN;
        data[1] = me->ch_cfg[KOZDEV_CHAN_ADC_BEG];
        data[2] = me->ch_cfg[KOZDEV_CHAN_ADC_END];
        data[3] = me->ch_cfg[KOZDEV_CHAN_ADC_TIMECODE];
        data[4] = PackMode(me->ch_cfg[KOZDEV_CHAN_ADC_GAIN],
                           me->ch_cfg[KOZDEV_CHAN_ADC_GAIN]);
        data[5] = 0; // Label
        DoDriverLog(me->devid, 0,
                    "%s:MULTICHAN beg=%d end=%d time=%d gain=%d", __FUNCTION__,
                    me->ch_cfg[KOZDEV_CHAN_ADC_BEG],
                    me->ch_cfg[KOZDEV_CHAN_ADC_END],
                    me->ch_cfg[KOZDEV_CHAN_ADC_TIMECODE],
                    me->ch_cfg[KOZDEV_CHAN_ADC_GAIN]);
    }
    else
    {
        dlc     = 4;
        data[0] = DESC_OSCILL;
        data[1] = me->ch_cfg[KOZDEV_CHAN_ADC_BEG] |
                  (me->ch_cfg[KOZDEV_CHAN_ADC_GAIN] << 6);
        data[2] = me->ch_cfg[KOZDEV_CHAN_ADC_TIMECODE];
        data[3] = PackMode(0, 0);
        DoDriverLog(me->devid, 0,
                    "%s:OSCILL chan=%d time=%d gain=%d", __FUNCTION__,
                    me->ch_cfg[KOZDEV_CHAN_ADC_BEG],
                    me->ch_cfg[KOZDEV_CHAN_ADC_TIMECODE],
                    me->ch_cfg[KOZDEV_CHAN_ADC_GAIN]);
    }

    me->lvmt->q_enqueue(me->handle, SQ_ALWAYS,
                        SQ_TRIES_ONS, 0,
                        NULL, NULL,
                        0, dlc, data);
}

static void DoSoftReset(privrec_t *me, int flags)
{
  int    x;
  
    if (flags & DSRF_RESET_CHANS)
    {
        me->ch_cfg[KOZDEV_CHAN_ADC_BEG]      = me->ch_beg;
        me->ch_cfg[KOZDEV_CHAN_ADC_END]      = me->ch_end;
        me->ch_cfg[KOZDEV_CHAN_ADC_TIMECODE] = me->timecode;
        me->ch_cfg[KOZDEV_CHAN_ADC_GAIN]     = me->gain;
    }
    
    if (flags & DSRF_RETCONFIGS)
        for (x = 0;  x < KOZDEV_CONFIG_CHAN_count;  x++)
            ReturnCtlCh(me, x);
}

//////////////////////////////////////////////////////////////////////

static void canadc40_ff (int devid, void *devptr, int is_a_reset);
static void canadc40_in (int devid, void *devptr,
                         int desc,  size_t dlc, uint8 *data);
static void canadc40_alv(int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr);

static int  canadc40_init_d(int devid, void *devptr,
                            int businfocount, int businfo[],
                            const char *auxinfo)
{
  privrec_t *me = devptr;
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               canadc40_ff, canadc40_in,
                               KOZDEV_NUMCHANS*2/*!!!*/,
                               CANKOZ_LYR_OPTION_NONE);
    if (me->handle < 0) return me->handle;
    me->lvmt->has_regs(me->handle, KOZDEV_CHAN_REGS_base);

    DoSoftReset(me, DSRF_RESET_CHANS);
    SetTmode   (me, KOZDEV_TMODE_NONE);

    sl_enq_tout_after(devid, devptr, ALIVE_USECS,     canadc40_alv, NULL);

    SetChanRDs       (devid, KOZDEV_CHAN_ADC_n_base, CANADC40_CHAN_ADC_n_count, 1000000.0, 0.0);
    SetChanReturnType(devid, KOZDEV_CHAN_ADC_n_base, CANADC40_CHAN_ADC_n_count, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, KOZDEV_CHAN_HW_VER,     1,                         IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_SW_VER,     1,                         IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}

static void canadc40_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s()", __FUNCTION__);
    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_DIR, 0,
                          NULL, NULL,
                          0, 1, DESC_STOP);
}

static void canadc40_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;
  int        hw_ver;
  int        sw_ver;

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, NULL);
    me->ch_cfg[KOZDEV_CHAN_HW_VER] = hw_ver; ReturnCtlCh(me, KOZDEV_CHAN_HW_VER);
    me->ch_cfg[KOZDEV_CHAN_SW_VER] = sw_ver; ReturnCtlCh(me, KOZDEV_CHAN_SW_VER);

    if (ADC_MODE_IS_NORM()) SendMULTICHAN(me);
    else /*!!!*/;

}

static void canadc40_alv(int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  privrec_t  *me     = (privrec_t *) devptr;

    sl_enq_tout_after(devid, devptr, ALIVE_USECS,     canadc40_alv, NULL);
    
    if (me->rd_rcvd == 0)
        me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT,
                          1, CANKOZ_DESC_GETDEVATTRS);
    
    me->rd_rcvd = 0;
}

static void canadc40_in (int devid, void *devptr,
                         int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me       = (privrec_t *) devptr;
  int         l;       // Line #
  int32       code;    // Code -- in device/Kozak encoding
  int32       val;     // Value -- in volts
  rflags_t    rflags;
  int         gain_code;

    switch (desc)
    {
        case DESC_MULTICHAN:
        case DESC_OSCILL:
        case DESC_READONECHAN:
            if (dlc < 5) return;
            me->rd_rcvd = 1;
            decode_attr(data[1], &l, &gain_code);
            rflags = kozadc_decode_le24(data + 2, gain_code, &code, &val);
            DoDriverLog(devid, 0 | DRIVERLOG_C_PKTINFO,
                        "in: RCHAN=%-2d code=0x%08X gain=%d val=%duV",
                        l, code, gain_code, val);
            if (l < 0  ||  l >= CANADC40_CHAN_ADC_n_count) return;
            ReturnInt32Datum(devid, KOZDEV_CHAN_ADC_n_base + l,
                             val, rflags);
            break;

        default:
            ////if (desc != 0xF8) DoDriverLog(devid, 0, "desc=%02x len=%d", desc, datasize);
            ;
    }
}

static void canadc40_rw_p(int devid, void *devptr,
                          int action,
                          int count, int *addrs,
                          cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

#if 1
  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int32      val;   // Value -- in volts

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

        if      (chn >= KOZDEV_CHAN_REGS_base  &&  chn <= KOZDEV_CHAN_REGS_last)
        {
            me->lvmt->regs_rw(me->handle, action, chn, &val);
        }
        else if (chn >= KOZDEV_CHAN_ADC_n_base  &&
                 chn <  KOZDEV_CHAN_ADC_n_base + CANADC40_CHAN_ADC_n_count)
        {
            /* Those are returned upon measurement */
        }
        else if (
                 ( // Is it a config channel?
                  chn >= KOZDEV_CONFIG_CHAN_base  &&
                  chn <  KOZDEV_CONFIG_CHAN_base + KOZDEV_CONFIG_CHAN_count
                 )
                 &&
                 (
                  action == DRVA_READ  ||  // Reads from any cfg channels...
                  !SUPPORTED_chans[chn]    // ...and any actions with unsupported ones
                 )
                )
            ReturnCtlCh(me, chn);          // Should be performed via cache
        /* Note: all config channels below shouldn't check "action",
                 since DRVA_READ was already processed above */
        /* Note: all config channels below shouldn't check "action",
                 since DRVA_READ was already processed above */
        else if (chn == KOZDEV_CHAN_ADC_BEG)
        {
            if (ADC_MODE_IS_NORM())
            {
                if (val < 0)
                    val = 0;
                if (val > CANADC40_CHAN_ADC_n_count - 1)
                    val = CANADC40_CHAN_ADC_n_count - 1;
                if (val > me->ch_cfg[KOZDEV_CHAN_ADC_END])
                    val = me->ch_cfg[KOZDEV_CHAN_ADC_END];
                me->ch_cfg[chn] = val;
                SendMULTICHAN(me);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_ADC_END)
        {
            if (ADC_MODE_IS_NORM())
            {
                if (val < 0)
                    val = 0;
                if (val > CANADC40_CHAN_ADC_n_count - 1)
                    val = CANADC40_CHAN_ADC_n_count - 1;
                if (val < me->ch_cfg[KOZDEV_CHAN_ADC_BEG])
                    val = me->ch_cfg[KOZDEV_CHAN_ADC_BEG];
                me->ch_cfg[chn] = val;
                SendMULTICHAN(me);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_ADC_TIMECODE)
        {
            if (ADC_MODE_IS_NORM())
            {
                if (val < MIN_TIMECODE)
                    val = MIN_TIMECODE;
                if (val > MAX_TIMECODE)
                    val = MAX_TIMECODE;
                me->ch_cfg[chn] = val;
                SendMULTICHAN(me);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_ADC_GAIN)
        {
            if (ADC_MODE_IS_NORM())
            {
                if (val < MIN_GAIN)
                    val = MIN_GAIN;
                if (val > MAX_GAIN)
                    val = MAX_GAIN;
                me->ch_cfg[chn] = val;
                SendMULTICHAN(me);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_DO_RESET)
        {
            if (val == CX_VALUE_COMMAND)
            {
                DoSoftReset(me, DSRF_RESET_CHANS | DSRF_RETCONFIGS);
                SendMULTICHAN(me);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_ADC_MODE)
        {
            /* These two are in fact read channels */
            ReturnCtlCh(me, chn);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }

#else
  int        n;
  int        x;
  int32      iv;
  rflags_t   zero_rflags = 0;
  cxdtype_t  dt          = CXDTYPE_INT32;
  int        nel         = 1;
  void      *nvp         = &iv;

fprintf(stderr, "%s(count=%d)\n", __FUNCTION__, count);
    for (n = 0;  n < count;  n++)
    {
        x = addrs[n];
        iv = x;
        ReturnDataSet(devid,
                      1,
                      &x, &dt, &nel,
                      &nvp, &zero_rflags, NULL);
    }
#endif
}


DEFINE_CXSD_DRIVER(canadc40, "CANADC40 driver",
                   NULL, NULL,
                   sizeof(privrec_t), canadc40_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   canadc40_init_d, canadc40_term_d, canadc40_rw_p);

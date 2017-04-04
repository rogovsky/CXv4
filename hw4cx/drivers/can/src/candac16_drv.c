#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/candac16_drv_i.h"


/*=== CANDAC16 specifics ===========================================*/

enum
{
    DEVTYPE   = 1, /* CANDAC16 is 1 */
};

enum
{
    DESC_WRITE_n_BASE  = 0,
    DESC_READ_n_BASE   = 0x10,

    DESC_FILE_WR_AT    = 0xF2,
    DESC_FILE_CREATE   = 0xF3,
    DESC_FILE_WR_SEQ   = 0xF4,
    DESC_FILE_CLOSE    = 0xF5,
    DESC_FILE_READ     = 0xF6,
    DESC_FILE_START    = 0xF7,

    DESC_U_FILE_STOP   = 0xFB,
    DESC_U_FILE_RESUME = 0xE7,
    DESC_U_FILE_PAUSE  = 0xEB,
    
    DESC_B_FILE_STOP   = 1,
    DESC_B_FILE_START  = 2,
    //DESC_B_ADC_STOP    = 3,
    //DESC_B_ADC_START   = 4,
    DESC_B_FILE_PAUSE  = 6,
    DESC_B_FILE_RESUME = 7,
    
    DAC_STAT_EXECING = 1 << 0,
    DAC_STAT_EXEC_RQ = 1 << 1,
    DAC_STAT_PAUSED  = 1 << 2,
    DAC_STAT_PAUS_RQ = 1 << 3,
    DAC_STAT_CONT_RQ = 1 << 4,
    DAC_STAT_NEXT_RQ = 1 << 5,
};

enum
{
    MIN_TIMECODE = 0,
    MAX_TIMECODE = 7,
    DEF_TIMECODE = 4,

    MIN_GAIN     = 0,
    MAX_GAIN     = 1,
    DEF_GAIN     = 0,

    TABLE_NO = 7,
    TABLE_ID = 15,
};

enum
{
    MAX_ALWD_VAL =   9999999,
    MIN_ALWD_VAL = -10000305
};

enum
{
    //XCANDAC16_MAX_NSTEPS     = 29/*!!!???*/,
    XCANDAC16_MAX_STEP_COUNT = 65535, // ==655.35s
    XCANDAC16_USECS_IN_STEP  = 10000,
};

enum
{
    HEARTBEAT_FREQ  = 10,
    HEARTBEAT_USECS = 1000000 / HEARTBEAT_FREQ,
};


static inline int32  kozdac_c16_to_val(uint32 kozak16)
{
    return scale32via64((int32)kozak16 - 0x8000, 10000000, 0x8000);
}

static inline uint32 kozdac_val_to_c16(int32 val)
{
    return scale32via64(val, 0x8000, 10000000) + 0x8000;
}

//////////////////////////////////////////////////////////////////////

#define SHOW_SET_IMMED 1

typedef struct
{
    int32  cur;  // Current value
    int32  spd;  // Speed
    int32  trg;  // Target

    int8   rcv;  // Current value was received
    int8   lkd;  // Is locked by table
    int8   isc;  // Is changing now
    int8   nxt;  // Next write may be performed (previous was acknowledged)
    int8   fst;  // This is the FirST step
    int8   fin;  // This is a FINal write
} cankoz_out_ch_t;

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int              num_isc;

    int32            ch_cfg[KOZDEV_CONFIG_CHAN_count];

    cankoz_out_ch_t  out[CANDAC16_CHAN_OUT_n_count];
} privrec_t;

static psp_paramdescr_t candac16_params[] =
{
    PSP_P_INT ("spd",    privrec_t, out[0].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd0",   privrec_t, out[0].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd1",   privrec_t, out[1].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd2",   privrec_t, out[2].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd3",   privrec_t, out[3].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd4",   privrec_t, out[4].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd5",   privrec_t, out[5].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd6",   privrec_t, out[6].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd7",   privrec_t, out[7].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd8",   privrec_t, out[8].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd9",   privrec_t, out[9].spd,  0, -20000000, +20000000),
    PSP_P_INT ("spd10",  privrec_t, out[10].spd, 0, -20000000, +20000000),
    PSP_P_INT ("spd11",  privrec_t, out[11].spd, 0, -20000000, +20000000),
    PSP_P_INT ("spd12",  privrec_t, out[12].spd, 0, -20000000, +20000000),
    PSP_P_INT ("spd13",  privrec_t, out[13].spd, 0, -20000000, +20000000),
    PSP_P_INT ("spd14",  privrec_t, out[14].spd, 0, -20000000, +20000000),
    PSP_P_INT ("spd15",  privrec_t, out[15].spd, 0, -20000000, +20000000),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

enum
{
    DSRF_RESET_CHANS = 1 << 0,
    DSRF_RETCONFIGS  = 1 << 1,
    DSRF_CUR2OUT     = 1 << 2,
};

static int8 SUPPORTED_chans[KOZDEV_CONFIG_CHAN_count] =
{
    [KOZDEV_CHAN_DO_RESET]     = 1,
    [KOZDEV_CHAN_OUT_MODE]     = 1,
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

static void DoSoftReset(privrec_t *me, int flags)
{
  int    x;
  
    /*!!! bzero() something? [*].nxt[]:=1? */

    if (flags & DSRF_RETCONFIGS)
        for (x = 0;  x < KOZDEV_CONFIG_CHAN_count;  x++)
            ReturnCtlCh(me, x);

    if (flags & DSRF_CUR2OUT)
        for (x = 0;  x < countof(me->out);  x++)
            ReturnInt32Datum(me->devid, KOZDEV_CHAN_OUT_n_base + x,
                             me->out[x].cur, 0);
}

static void SendWrRq(privrec_t *me, int l, int32 val)
{
  uint32      code;    // Code -- in device/Kozak encoding
  
    code = kozdac_val_to_c16(val);
    DoDriverLog(me->devid, 0 | DRIVERLOG_C_DATACONV,
                "%s: w=%d => [%d]:=0x%04X", __FUNCTION__, val, l, code);

    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_ONS, 0,
                          NULL, NULL,
                          0, 5,
                          DESC_WRITE_n_BASE + l,
                          code        & 0xFF,
                          (code >> 8) & 0xFF,
                          0,
                          0); /*!!! And should REPLACE if such packet is present...  */
}

static void SendRdRq(privrec_t *me, int l)
{
    me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, DESC_READ_n_BASE + l);
}

//////////////////////////////////////////////////////////////////////

static void candac16_ff (int devid, void *devptr, int is_a_reset);
static void candac16_in (int devid, void *devptr,
                         int desc,  size_t dlc, uint8 *data);
static void candac16_hbt(int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr);

static int  candac16_init_d(int devid, void *devptr,
                            int businfocount, int businfo[],
                            const char *auxinfo)
{
  privrec_t *me = devptr;
  int        l;       // Line #
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    if (me->out[0].spd != 0)
        for (l = 1;  l < countof(me->out);  l++)
            if (me->out[l].spd == 0)
                me->out[l].spd = me->out[0].spd;

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               candac16_ff, candac16_in,
                               KOZDEV_NUMCHANS*2/*!!!*/);
    if (me->handle < 0) return me->handle;
    me->lvmt->has_regs(me->handle, KOZDEV_CHAN_REGS_base);

    DoSoftReset(me, DSRF_RESET_CHANS);
    SetTmode   (me, KOZDEV_TMODE_NONE);

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, candac16_hbt, NULL);

    SetChanRDs       (devid, KOZDEV_CHAN_OUT_n_base,      countof(me->out), 1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_RATE_n_base, countof(me->out), 1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out), 1000000.0, 0.0);
    SetChanReturnType(devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out), IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_HW_VER,          1,                IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_SW_VER,          1,                IS_AUTOUPDATED_TRUSTED);
    SetChanQuant     (devid, KOZDEV_CHAN_OUT_n_base,      countof(me->out), (CxAnyVal_t){.i32=305}, CXDTYPE_INT32);
    SetChanQuant     (devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out), (CxAnyVal_t){.i32=305}, CXDTYPE_INT32);

    return DEVSTATE_OPERATING;
}

static void candac16_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s()", __FUNCTION__);
}

static void candac16_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;
  int        l;       // Line #
  int        hw_ver;
  int        sw_ver;

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, NULL);
    me->ch_cfg[KOZDEV_CHAN_HW_VER] = hw_ver; ReturnCtlCh(me, KOZDEV_CHAN_HW_VER);
    me->ch_cfg[KOZDEV_CHAN_SW_VER] = sw_ver; ReturnCtlCh(me, KOZDEV_CHAN_SW_VER);

    if (is_a_reset)
    {
        for (l = 0;  l < countof(me->out);  l++) me->out[l].rcv = 0;
        DoSoftReset(me, 0);
    }
}

static void candac16_hbt(int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         l;       // Line #
  int32       val;     // Value -- in volts

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, candac16_hbt, NULL);

    /*  */
    if (me->num_isc > 0)
        for (l = 0;  l < countof(me->out);  l++)
            if (me->out[l].isc  &&  me->out[l].nxt)
            {
                /* Guard against dropping speed to 0 in process */
                if      (me->out[l].spd == 0)
                {
                    val =  me->out[l].trg;
                    me->out[l].fin = 1;
                }
                /* We use separate branches for ascend and descend
                   to simplify arithmetics and escape juggling with sign */
                else if (me->out[l].trg > me->out[l].cur)
                {
                    val = me->out[l].cur + abs(me->out[l].spd) / HEARTBEAT_FREQ;
                    if (val >= me->out[l].trg)
                    {
                        val =  me->out[l].trg;
                        me->out[l].fin = 1;
                    }
                }
                else
                {
                    val = me->out[l].cur - abs(me->out[l].spd) / HEARTBEAT_FREQ;
                    if (val <= me->out[l].trg)
                    {
                        val =  me->out[l].trg;
                        me->out[l].fin = 1;
                    }
                }

                SendWrRq(me, l, val);
                SendRdRq(me, l);
                me->out[l].nxt = 0; // Drop the "next may be sent..." flag
            }

#if 0
    /* Request current values of table-driven channels */
    if (me->t_mode == TMODE_RUNN)
        for (l = 0;  l < XCANDAC16_CHAN_OUT_n_COUNT;  l++)
            if (me->t_aff_c[l]) SendRdRq(me, l);
#endif
}

static void candac16_in (int devid, void *devptr,
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
        case DESC_READ_n_BASE ... DESC_READ_n_BASE + CANDAC16_CHAN_OUT_n_count-1:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 3) return;

            l = desc - DESC_READ_n_BASE;
            code   = data[1] + (data[2] << 8);
            val    = kozdac_c16_to_val(code);
            DoDriverLog(devid, 0 | DRIVERLOG_C_PKTINFO,
                        "in: WCHAN=%d code=0x%04X val=%duv",
                        l, code, val);
            me->out[l].cur = val;
            me->out[l].rcv = 1;
            me->out[l].nxt = 1;

            if (me->out[l].fin)
            {
                if (me->out[l].isc) me->num_isc--;
                me->out[l].isc = 0;
            }

            ReturnInt32Datum(devid, KOZDEV_CHAN_OUT_CUR_n_base + l,
                             me->out[l].cur, 0);
            if (!me->out[l].isc  ||  !SHOW_SET_IMMED)
                ReturnInt32Datum(devid, KOZDEV_CHAN_OUT_n_base + l,
                                 me->out[l].cur, 0);
            else if (me->out[l].fst)
            {
                me->out[l].fst = 0;
                val = kozdac_c16_to_val(kozdac_val_to_c16(me->out[l].trg));
                ReturnInt32Datum(devid, KOZDEV_CHAN_OUT_n_base + l,
                                 val, 0);
            }

            break;

        default:
            ////if (desc != 0xF8) DoDriverLog(devid, 0, "desc=%02x len=%d", desc, datasize);
            ;
    }
}

static void candac16_rw_p(int devid, void *devptr,
                          int action,
                          int count, int *addrs,
                          cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

#if 1
  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int        l;     // Line #
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
        else if (chn == KOZDEV_CHAN_DO_RESET)
        {
            if (val == CX_VALUE_COMMAND)
            {
                DoSoftReset(me, DSRF_RESET_CHANS | DSRF_RETCONFIGS | DSRF_CUR2OUT);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_OUT_MODE)
        {
            /* These two are in fact read channels */
            ReturnCtlCh(me, chn);
        }
        else if (chn >= KOZDEV_CHAN_OUT_n_base       &&
                 chn <  KOZDEV_CHAN_OUT_n_base      + CANDAC16_CHAN_OUT_n_count)
        {
            //if (action == DRVA_WRITE) fprintf(stderr, "\t[%d]:=%d\n", chn, val);
            l = chn - KOZDEV_CHAN_OUT_n_base;
            /* May we touch this channel now? */
            if (me->out[l].lkd) goto NEXT_CHANNEL;
            
            if (action == DRVA_WRITE)
            {
                /* Convert volts to code, preventing integer overflow/slip */
                if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                
                /* ...and how should we perform the change: */
                if (!me->out[l].isc  &&  // No mangling with now-changing channels...
                    (
                     /* No speed limit? */
                     me->out[l].spd == 0
                     ||
                     /* Or is it an absolute-value-decrement? */
                     (
                      me->out[l].spd > 0  &&
                      abs(val) < abs(me->out[l].cur)
                     )
                     ||
                     /* Or is this step less than speed? */
                     (
                      abs(val - me->out[l].cur) < abs(me->out[l].spd) / HEARTBEAT_FREQ
                     )
                    )
                   )
                /* Just do write?... */
                {
                    SendWrRq(me, l, val);
                }
                else
                /* ...or initiate slow change? */
                {
                    if (me->out[l].isc == 0) me->num_isc++;
                    me->out[l].trg = val;
                    me->out[l].isc = 1;
                    me->out[l].fst = 1;
                    me->out[l].fin = 0;
                }
            }

            /* Perform read anyway */
            SendRdRq(me, l);
        }
        else if (chn >= KOZDEV_CHAN_OUT_RATE_n_base  &&
                 chn <  KOZDEV_CHAN_OUT_RATE_n_base + CANDAC16_CHAN_OUT_n_count)
        {
            l = chn - KOZDEV_CHAN_OUT_RATE_n_base;
            if (action == DRVA_WRITE)
            {
                /* Note: no bounds-checking for value, since it isn't required */
                if (abs(val) < 305/*!!!*/) val = 0;
                me->out[l].spd = val;
            }
            ReturnInt32Datum(devid, chn, me->out[l].spd, 0);
        }
        else if (chn >= KOZDEV_CHAN_OUT_CUR_n_base   &&
                 chn <  KOZDEV_CHAN_OUT_CUR_n_base  + CANDAC16_CHAN_OUT_n_count)
        {
            l = chn - KOZDEV_CHAN_OUT_CUR_n_base;
            if (me->out[l].rcv)
                ReturnInt32Datum(devid, chn, me->out[l].cur, 0);
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


DEFINE_CXSD_DRIVER(candac16, "CANDAC16 driver",
                   NULL, NULL,
                   sizeof(privrec_t), candac16_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   candac16_init_d, candac16_term_d, candac16_rw_p);

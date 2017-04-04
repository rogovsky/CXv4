#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/cdac20_drv_i.h"


/*=== CDAC20 specifics =============================================*/

enum
{
    DEVTYPE_CDAC20  = 3,
    DEVTYPE_CEAC51  = 18,
    DEVTYPE_CEAC51A = 45,
};

enum
{
    DESC_STOP          = 0x00,
    DESC_MULTICHAN     = 0x01,
    DESC_OSCILL        = 0x02,
    DESC_READONECHAN   = 0x03,
    DESC_READRING      = 0x04,
    DESC_WRITEDAC     = 0x05,
    DESC_READDAC      = 0x06,
    DESC_CALIBRATE    = 0x07,
    DESC_WR_DAC_n_BASE = 0x80,
    DESC_RD_DAC_n_BASE = 0x90,

    DESC_DIGCORR_SET  = 0xE0,
    DESC_DIGCORR_STAT = 0xE1,

    DESC_FILE_WR_AT    = 0xF2,
    DESC_FILE_CREATE   = 0xF3,
    DESC_FILE_WR_SEQ   = 0xF4,
    DESC_FILE_CLOSE    = 0xF5,
    DESC_FILE_READ     = 0xF6,
    DESC_FILE_START    = 0xF7,

    DESC_U_FILE_STOP   = 0xFB,
    DESC_U_FILE_RESUME = 0xE7,
    DESC_U_FILE_PAUSE  = 0xEB,
    
    DESC_GET_DAC_STAT  = 0xFD,

    DESC_B_FILE_STOP   = 1,
    DESC_B_FILE_START  = 2,
    DESC_B_ADC_STOP    = 3,
    DESC_B_ADC_START   = 4,
    DESC_B_FILE_PAUSE  = 6,
    DESC_B_FILE_RESUME = 7,
    
    MODE_BIT_LOOP = 1 << 4,
    MODE_BIT_SEND = 1 << 5,

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
    XCDAC20_MAX_NSTEPS     = 29,
    XCDAC20_MAX_STEP_COUNT = 65535, // ==655.35s
    XCDAC20_USECS_IN_STEP  = 10000,
};

enum
{
    HEARTBEAT_FREQ  = 10,
    HEARTBEAT_USECS = 1000000 / HEARTBEAT_FREQ,
        
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

static inline int32 cdac20_daccode_to_val(uint32 kozak32)
{
    return scale32via64((int32)kozak32 - 0x800000, 10000000, 0x800000);
}

static inline int32 cdac20_val_to_daccode(int32 val)
{
    return scale32via64(val, 0x800000, 10000000) + 0x800000;
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

    int              devcode;

    int              rd_rcvd;
    
    int              ch_beg;
    int              ch_end;
    int              timecode;
    int              gain;
    int              fast;

    int              num_isc;

    int32            autocalb_onoff;
    int32            autocalb_secs;
    struct timeval   last_calibrate_time;

    int32            ch_cfg[KOZDEV_CONFIG_CHAN_count];

    int32            min_alwd;
    int32            max_alwd;

    cankoz_out_ch_t  out[CDAC20_CHAN_OUT_n_count];
} privrec_t;

static psp_paramdescr_t cdac20_params[] =
{
    PSP_P_INT ("beg",  privrec_t, ch_beg,   0,                         0, CDAC20_CHAN_ADC_n_count-1),
    PSP_P_INT ("end",  privrec_t, ch_end,   CDAC20_CHAN_ADC_n_count-1, 0, CDAC20_CHAN_ADC_n_count-1),
    PSP_P_INT ("adc_timecode", privrec_t, timecode, DEF_TIMECODE,      MIN_TIMECODE, MAX_TIMECODE),
    PSP_P_INT ("gain", privrec_t, gain,     DEF_GAIN,                  MIN_GAIN,     MAX_GAIN),
    PSP_P_FLAG("fast", privrec_t, fast,     1, 0),
    PSP_P_INT ("spd",  privrec_t, out[0].spd, 0, -20000000, +20000000),
    PSP_P_INT ("min",  privrec_t, min_alwd,   0, MIN_ALWD_VAL, MAX_ALWD_VAL),
    PSP_P_INT ("max",  privrec_t, max_alwd,   0, MIN_ALWD_VAL, MAX_ALWD_VAL),
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
    [KOZDEV_CHAN_ADC_MODE]     = 1,
    [KOZDEV_CHAN_ADC_BEG]      = 1,
    [KOZDEV_CHAN_ADC_END]      = 1,
    [KOZDEV_CHAN_ADC_TIMECODE] = 1,
    [KOZDEV_CHAN_ADC_GAIN]     = 1,
    [KOZDEV_CHAN_OUT_MODE]     = 1,
    [KOZDEV_CHAN_HW_VER]       = 1,
    [KOZDEV_CHAN_SW_VER]       = 1,

    [KOZDEV_CHAN_DO_CALB_DAC]    = 1,
    [KOZDEV_CHAN_AUTOCALB_ONOFF] = 1,
    [KOZDEV_CHAN_AUTOCALB_SECS]  = 1,
    [KOZDEV_CHAN_DIGCORR_MODE]   = 1,
    [KOZDEV_CHAN_DIGCORR_VALID]  = 1,
    [KOZDEV_CHAN_DIGCORR_FACTOR] = 1,
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
  
    code = cdac20_val_to_daccode(val);
    DoDriverLog(me->devid, 0 | DRIVERLOG_C_DATACONV,
                "%s: w=%d => [%d]:=0x%06X", __FUNCTION__, val, l, code);

    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_ONS, 0,
                          NULL, NULL,
                          0, 7,
                          DESC_WRITEDAC,
                          code         & 0xFF,
                          (code >> 8)  & 0xFF,
                          (code >> 16) & 0xFF,
                          0,
                          0,
                          0); /*!!! And should REPLACE if such packet is present...  */
}

static void SendRdRq(privrec_t *me, int l)
{
    me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, DESC_READDAC);
}

//////////////////////////////////////////////////////////////////////

static void cdac20_ff (int devid, void *devptr, int is_a_reset);
static void cdac20_in (int devid, void *devptr,
                       int desc,  size_t dlc, uint8 *data);
static void cdac20_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values);
static void cdac20_alv(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr);
static void cdac20_hbt(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr);

static int  cdac20_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t *me = devptr;
  int        l;       // Line #
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    for (l = 1;  l < countof(me->out);  l++)
        me->out[l].spd = me->out[0].spd;

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE_CDAC20,
                               cdac20_ff, cdac20_in,
                               KOZDEV_NUMCHANS*2/*!!!*/);
    if (me->handle < 0) return me->handle;
    me->lvmt->add_devcode(me->handle, DEVTYPE_CEAC51);
    me->lvmt->add_devcode(me->handle, DEVTYPE_CEAC51A);
    me->lvmt->has_regs(me->handle, KOZDEV_CHAN_REGS_base);

    DoSoftReset(me, DSRF_RESET_CHANS);
    SetTmode   (me, KOZDEV_TMODE_NONE);

    sl_enq_tout_after(devid, devptr, ALIVE_USECS,     cdac20_alv, NULL);
    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, cdac20_hbt, NULL);

    SetChanRDs       (devid, KOZDEV_CHAN_OUT_n_base,      countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_RATE_n_base, countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_ADC_n_base,      CDAC20_CHAN_ADC_n_count, 1000000.0, 0.0);
    SetChanReturnType(devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out),        IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_ADC_n_base,      CDAC20_CHAN_ADC_n_count, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, KOZDEV_CHAN_HW_VER,          1,                       IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_SW_VER,          1,                       IS_AUTOUPDATED_TRUSTED);
    SetChanQuant     (devid, KOZDEV_CHAN_OUT_n_base,      countof(me->out), (CxAnyVal_t){.i32=305}, CXDTYPE_INT32);
    SetChanQuant     (devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out), (CxAnyVal_t){.i32=305}, CXDTYPE_INT32);

    return DEVSTATE_OPERATING;
}

static void cdac20_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s()", __FUNCTION__);
    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_DIR, 0,
                          NULL, NULL,
                          0, 1, DESC_STOP);
}

static void cdac20_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;
  int        l;       // Line #
  int        hw_ver;
  int        sw_ver;

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, &(me->devcode));
    me->ch_cfg[KOZDEV_CHAN_HW_VER] = hw_ver; ReturnCtlCh(me, KOZDEV_CHAN_HW_VER);
    me->ch_cfg[KOZDEV_CHAN_SW_VER] = sw_ver; ReturnCtlCh(me, KOZDEV_CHAN_SW_VER);

    if (ADC_MODE_IS_NORM()) SendMULTICHAN(me);
    else /*!!!*/;

    if (is_a_reset)
    {
        for (l = 0;  l < countof(me->out);  l++) me->out[l].rcv = 0;
        DoSoftReset(me, 0);
    }
}

static void cdac20_alv(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr)
{
  privrec_t  *me     = (privrec_t *) devptr;

    sl_enq_tout_after(devid, devptr, ALIVE_USECS,     cdac20_alv, NULL);
    
    if (me->rd_rcvd == 0)
        me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT,
                          1, CANKOZ_DESC_GETDEVATTRS);
    
    me->rd_rcvd = 0;
}

static void cdac20_hbt(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         l;       // Line #
  int32       val;     // Value -- in volts
  struct timeval  now;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, cdac20_hbt, NULL);

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
        for (l = 0;  l < XCDAC20_CHAN_OUT_n_COUNT;  l++)
            if (me->t_aff_c[l]) SendRdRq(me, l);
#endif

    /* Auto-calibration */
    if (me->autocalb_onoff  &&  me->autocalb_secs > 0)
    {
        gettimeofday(&now, NULL);
        now.tv_sec -= me->autocalb_secs;
        if (TV_IS_AFTER(now, me->last_calibrate_time))
        {
            cdac20_rw_p(devid, me,
                        DRVA_WRITE,
                        1,
                        (int[])      {KOZDEV_CHAN_DO_CALB_DAC},
                        (cxdtype_t[]){CXDTYPE_INT32},
                        (int[])      {1},
                        (void*[])    {(int32[]){CX_VALUE_COMMAND}});
        }
    }
}

static void cdac20_in (int devid, void *devptr,
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
            if (l < 0  ||  l >= CDAC20_CHAN_ADC_n_count) return;
            ReturnInt32Datum(devid, KOZDEV_CHAN_ADC_n_base + l,
                             val, rflags);
            break;

        case DESC_READDAC:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 4) return;

            l = desc - DESC_READDAC;
            code   = data[1] + (data[2] << 8) + (data[3] << 16);
            val    = cdac20_daccode_to_val(code);
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
                val = cdac20_daccode_to_val(cdac20_val_to_daccode(me->out[l].trg));
                ReturnInt32Datum(devid, KOZDEV_CHAN_OUT_n_base + l,
                                 val, 0);
            }

            break;

        case CANKOZ_DESC_GETDEVSTAT:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            break;
            
        case DESC_DIGCORR_STAT:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 5) return;

            code = data[4] + (data[3] << 8) + (data[2] << 16);
            if (code & 0x00800000) code -= 0x01000000;
            val  = code; // That's in unknown encoding, for Kozak only
            DoDriverLog(devid, 0 | DRIVERLOG_C_PKTINFO,
                        "DIGCORR_STAT: mode=%d code=0x%04X",
                        data[1], code);
            ReturnInt32Datum(devid, KOZDEV_CHAN_DIGCORR_MODE,   data[1]       & 1, 0);
            ReturnInt32Datum(devid, KOZDEV_CHAN_DIGCORR_VALID, (data[1] >> 1) & 1, 0);
            ReturnInt32Datum(devid, KOZDEV_CHAN_DIGCORR_FACTOR, code,              0);
            break;
            
        default:
            ////if (desc != 0xF8) DoDriverLog(devid, 0, "desc=%02x len=%d", desc, datasize);
            ;
    }
}

static void cdac20_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

#if 1
  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int          l;     // Line #
  int32        val;   // Value -- in volts
  sq_status_t  sr;

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
                 chn <  KOZDEV_CHAN_ADC_n_base + CDAC20_CHAN_ADC_n_count)
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
                if (val > CDAC20_CHAN_ADC_n_count - 1)
                    val = CDAC20_CHAN_ADC_n_count - 1;
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
                if (val > CDAC20_CHAN_ADC_n_count - 1)
                    val = CDAC20_CHAN_ADC_n_count - 1;
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
                DoSoftReset(me, DSRF_RESET_CHANS | DSRF_RETCONFIGS | DSRF_CUR2OUT);
                SendMULTICHAN(me);
            }
            ReturnCtlCh(me, chn);
        }
        else if (chn == KOZDEV_CHAN_ADC_MODE  ||  chn == KOZDEV_CHAN_OUT_MODE)
        {
            /* These two are in fact read channels */
            ReturnCtlCh(me, chn);
        }

        else if (chn == KOZDEV_CHAN_DO_CALB_DAC  &&
                 (me->devcode == DEVTYPE_CDAC20  ||
                  me->devcode == DEVTYPE_CEAC51))
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                me->lvmt->q_enq_ons_v(me->handle, SQ_IF_ABSENT,
                                      2, DESC_CALIBRATE, 0);
                gettimeofday(&(me->last_calibrate_time), NULL);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == KOZDEV_CHAN_DIGCORR_MODE  &&
                 (me->devcode == DEVTYPE_CDAC20  ||
                  me->devcode == DEVTYPE_CEAC51))
        {
            //fprintf(stderr, "%s(XCDAC20_CHAN_DIGCORR_MODE:%d)\n", __FUNCTION__, action);
            sr = SQ_NOTFOUND;
            if (action == DRVA_WRITE)
            {
                val = val != 0;
                me->ch_cfg[chn] = val;

                me->lvmt->q_enq_ons_v(me->handle, SQ_IF_ABSENT,
                                      3, DESC_DIGCORR_SET, me->ch_cfg[chn], 0);
            }
            if (sr == SQ_NOTFOUND)
                me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, DESC_DIGCORR_STAT);
        }
        else if ((chn == KOZDEV_CHAN_DIGCORR_VALID  ||
                  chn == KOZDEV_CHAN_DIGCORR_FACTOR)  &&
                 (me->devcode == DEVTYPE_CDAC20  ||
                  me->devcode == DEVTYPE_CEAC51))
        {
            //fprintf(stderr, "%s(XCDAC20_CHAN_DIGCORR_V:%d)\n", __FUNCTION__, action);
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, DESC_DIGCORR_STAT);
        }
        else if (chn == KOZDEV_CHAN_AUTOCALB_ONOFF)
        {
            if (action == DRVA_WRITE)
            {
                val = val != 0;
                me->autocalb_onoff = val;
            }
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == KOZDEV_CHAN_AUTOCALB_SECS)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 0)         val = 0;
                if (val > 86400*365) val = 86400*365;
                me->autocalb_secs = val;
            }
            ReturnInt32Datum(devid, chn, val, 0);
        }

        else if (chn >= KOZDEV_CHAN_OUT_n_base       &&
                 chn <  KOZDEV_CHAN_OUT_n_base      + CDAC20_CHAN_OUT_n_count)
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

                if (me->min_alwd < me->max_alwd)
                {
                    if (val > me->max_alwd) val = me->max_alwd;
                    if (val < me->min_alwd) val = me->min_alwd;
                }

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
                 chn <  KOZDEV_CHAN_OUT_RATE_n_base + CDAC20_CHAN_OUT_n_count)
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
                 chn <  KOZDEV_CHAN_OUT_CUR_n_base  + CDAC20_CHAN_OUT_n_count)
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


DEFINE_CXSD_DRIVER(cdac20, "CDAC20/CEAC51/CEAC51A CAN-DAC/ADC driver",
                   NULL, NULL,
                   sizeof(privrec_t), cdac20_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   cdac20_init_d, cdac20_term_d, cdac20_rw_p);

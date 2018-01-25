#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "cankoz_lyr.h"
#include "advdac.h"

#include "drv_i/cac208_drv_i.h"


/*=== CAC208 specifics =============================================*/

enum
{
    DEVTYPE   = 4, /* CAC208 is 4 */
};

enum
{
    DESC_STOP          = 0x00,
    DESC_MULTICHAN     = 0x01,
    DESC_OSCILL        = 0x02,
    DESC_READONECHAN   = 0x03,
    DESC_READRING      = 0x04,
    DESC_WR_DAC_n_BASE = 0x80,
    DESC_RD_DAC_n_BASE = 0x90,

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

    TABLE_NO     = 0,
    DEF_TABLE_ID = 15,
};

enum
{
    MAX_ALWD_VAL =   9999999,
    MIN_ALWD_VAL = -10000305,
    THE_QUANT    = 305,
};

enum
{
    CAC208_TABLE_MAX_NSTEPS     = 31,
    CAC208_TABLE_MAX_STEP_COUNT = 65535, // ==655.35s
    CAC208_TABLE_STEPS_PER_SEC  = 100,
    CAC208_TABLE_USECS_IN_STEP  = 1000000 / CAC208_TABLE_STEPS_PER_SEC, // ==10000
    CAC208_TABLE_MAX_FILE_BYTES = (CAC208_TABLE_MAX_NSTEPS-1) * (4 * CAC208_CHAN_OUT_n_count + 2), // http://www.inp.nsk.su/~kozak/designs/cac208r.pdf page 16

    CAC208_TABLE_VAL_DISABLE_CHAN   = -20000000,
    CAC208_TABLE_VAL_START_FROM_CUR = +20000000,
};

enum
{
    HEARTBEAT_FREQ  = 10,
    HEARTBEAT_USECS = 1000000 / HEARTBEAT_FREQ,
        
    ALIVE_SECONDS   = 60,
    ALIVE_USECS     = ALIVE_SECONDS * 1000000,
};

enum
{
    DEVSPEC_CHAN_ADC_n_count         = CAC208_CHAN_ADC_n_count,
    DEVSPEC_CHAN_OUT_n_count         = CAC208_CHAN_OUT_n_count,

    DEVSPEC_CHAN_OUT_IMM_n_base      = CAC208_CHAN_OUT_IMM_n_base,
    DEVSPEC_CHAN_OUT_TAC_n_base      = CAC208_CHAN_OUT_TAC_n_base,

    DEVSPEC_TABLE_MAX_NSTEPS         = CAC208_TABLE_MAX_NSTEPS,
    DEVSPEC_TABLE_MAX_STEP_COUNT     = CAC208_TABLE_MAX_STEP_COUNT,
    DEVSPEC_TABLE_STEPS_PER_SEC      = CAC208_TABLE_STEPS_PER_SEC,
    DEVSPEC_TABLE_USECS_IN_STEP      = CAC208_TABLE_USECS_IN_STEP,
    DEVSPEC_TABLE_MAX_FILE_BYTES     = CAC208_TABLE_MAX_FILE_BYTES,

    DEVSPEC_TABLE_VAL_DISABLE_CHAN   = CAC208_TABLE_VAL_DISABLE_CHAN,
    DEVSPEC_TABLE_VAL_START_FROM_CUR = CAC208_TABLE_VAL_START_FROM_CUR,
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

static inline int32  kozdac_c16_to_val(uint32 kozak16)
{
    return scale32via64((int32)kozak16 - 0x8000, 10000000, 0x8000);
}

static inline uint32 kozdac_val_to_c16(int32 val)
{
    return scale32via64(val, 0x8000, 10000000) + 0x8000;
}

static inline int32 val_to_daccode_to_val(int32 val)
{
    return kozdac_c16_to_val(kozdac_val_to_c16(val));
}

//////////////////////////////////////////////////////////////////////

#define SHOW_SET_IMMED 1
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

    int              num_isc;
    int              supports_unicast_t_ctl;

    int32            ch_cfg[KOZDEV_CONFIG_CHAN_count];

    advdac_out_ch_t  out     [DEVSPEC_CHAN_OUT_n_count];

    int              t_times_nsteps;
    int32            t_times                           [DEVSPEC_TABLE_MAX_NSTEPS];
    int32            t_vals  [DEVSPEC_CHAN_OUT_n_count][DEVSPEC_TABLE_MAX_NSTEPS];
    int              t_nsteps[DEVSPEC_CHAN_OUT_n_count];
    int              t_mode;
    int              t_GET_DAC_STAT_hbt_ctr;
    int              t_file_bytes; // Note: signed (and 32-bit) is OK for Kozak tables; but NOT for something bigger
    uint8            t_file[DEVSPEC_TABLE_MAX_FILE_BYTES];
} privrec_t;

#define ONE_LINE_PARAMS(lab, line) \
    PSP_P_INT ("spd"lab, privrec_t, out[line].spd,  0, -20000000,    +20000000),    \
    PSP_P_INT ("tac"lab, privrec_t, out[line].tac, -1, -1,           1000),         \
    PSP_P_INT ("min"lab, privrec_t, out[line].min,  0, MIN_ALWD_VAL, MAX_ALWD_VAL), \
    PSP_P_INT ("max"lab, privrec_t, out[line].max,  0, MIN_ALWD_VAL, MAX_ALWD_VAL)

static psp_paramdescr_t cac208_params[] =
{
    PSP_P_INT ("beg",  privrec_t, ch_beg,   0,                         0, CAC208_CHAN_ADC_n_count-1),
    PSP_P_INT ("end",  privrec_t, ch_end,   CAC208_CHAN_ADC_n_count-1, 0, CAC208_CHAN_ADC_n_count-1),
    PSP_P_INT ("adc_timecode", privrec_t, timecode, DEF_TIMECODE,              MIN_TIMECODE, MAX_TIMECODE),
    PSP_P_INT ("gain", privrec_t, gain,     DEF_GAIN,                  MIN_GAIN,     MAX_GAIN),
    PSP_P_FLAG("fast", privrec_t, fast,     1, 0),
    ONE_LINE_PARAMS(  "", 0),
    ONE_LINE_PARAMS( "0", 0),
    ONE_LINE_PARAMS( "1", 1),
    ONE_LINE_PARAMS( "2", 2),
    ONE_LINE_PARAMS( "3", 3),
    ONE_LINE_PARAMS( "4", 4),
    ONE_LINE_PARAMS( "5", 5),
    ONE_LINE_PARAMS( "7", 6),
    ONE_LINE_PARAMS( "7", 7),
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
    [KOZDEV_CHAN_ADC_BEG]      = 1,
    [KOZDEV_CHAN_ADC_END]      = 1,
    [KOZDEV_CHAN_ADC_TIMECODE] = 1,
    [KOZDEV_CHAN_ADC_GAIN]     = 1,
    [KOZDEV_CHAN_HW_VER]       = 1,
    [KOZDEV_CHAN_SW_VER]       = 1,
    [KOZDEV_CHAN_ADC_MODE]     = 1,
    [KOZDEV_CHAN_OUT_MODE]     = 1,
    [KOZDEV_CHAN___T_MODE]     = 1,

    [KOZDEV_CHAN_DO_TAB_DROP]     = 1,
    [KOZDEV_CHAN_DO_TAB_ACTIVATE] = 1,
    [KOZDEV_CHAN_DO_TAB_START]    = 1,
    [KOZDEV_CHAN_DO_TAB_STOP]     = 1,
    [KOZDEV_CHAN_DO_TAB_PAUSE]    = 1,
    [KOZDEV_CHAN_DO_TAB_RESUME]   = 1,
    [KOZDEV_CHAN_OUT_TAB_ID]      = 1,
};

#define ADC_MODE_IS_NORM() \
    (me->ch_cfg[KOZDEV_CHAN_ADC_MODE-KOZDEV_CONFIG_CHAN_base] == KOZDEV_ADC_MODE_NORM)

#define TAB_MODE_IS_NORM() \
    (me->t_mode < TMODE_LOAD)

static void ReturnCtlCh(privrec_t *me, int chan)
{
    ReturnInt32Datum(me->devid, chan, me->ch_cfg[chan],
                     SUPPORTED_chans[chan]? 0 : CXRF_UNSUPPORTED);
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
  
    code = kozdac_val_to_c16(val);
    DoDriverLog(me->devid, 0 | DRIVERLOG_C_DATACONV,
                "%s: w=%d => [%d]:=0x%04X", __FUNCTION__, val, l, code);

    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_ONS, 0,
                          NULL, NULL,
                          0, 5,
                          DESC_WR_DAC_n_BASE + l,
                          (code >> 8) & 0xFF,
                          code & 0xFF,
                          0,
                          0); /*!!! And should REPLACE if such packet is present...  */
}

static void SendRdRq(privrec_t *me, int l)
{
    me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, DESC_RD_DAC_n_BASE + l);
}

//////////////////////////////////////////////////////////////////////
#include "advdac_cankoz_table_meat.h"
#include "advdac_slowmo_kinetic_meat.h"
//////////////////////////////////////////////////////////////////////

static void cac208_ff (int devid, void *devptr, int is_a_reset);
static void cac208_in (int devid, void *devptr,
                       int desc,  size_t dlc, uint8 *data);
static void cac208_alv(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr);
static void cac208_hbt(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr);

static int  cac208_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t *me = devptr;
  int        l;       // Line #
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    for (l = 1;  l < countof(me->out);  l++)
    {
        if (me->out[l].spd == 0)
            me->out[l].spd = me->out[0].spd;
        if (me->out[l].tac <  0)
            me->out[l].tac = me->out[0].tac;
        if (me->out[l].min == 0)
            me->out[l].min = me->out[0].min;
        if (me->out[l].max == 0)
            me->out[l].max = me->out[0].max;
    }

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               cac208_ff, cac208_in,
                               KOZDEV_NUMCHANS*2/*!!!*/ +
                               (DEVSPEC_TABLE_MAX_FILE_BYTES / 7 + 1)*2 /* *2 because of GETDEVSTAT packets */);
    if (me->handle < 0) return me->handle;
    me->lvmt->has_regs(me->handle, KOZDEV_CHAN_REGS_base);

    DoSoftReset(me, DSRF_RESET_CHANS);
    SetTmode   (me, KOZDEV_TMODE_NONE, NULL);

    sl_enq_tout_after(devid, devptr, ALIVE_USECS,     cac208_alv, NULL);
    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, cac208_hbt, NULL);

    SetChanRDs       (devid, KOZDEV_CHAN_OUT_n_base,      countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_RATE_n_base, countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, DEVSPEC_CHAN_OUT_IMM_n_base, countof(me->out),        1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_ADC_n_base,      CAC208_CHAN_ADC_n_count, 1000000.0, 0.0);
    SetChanReturnType(devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out),        IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_ADC_n_base,      CAC208_CHAN_ADC_n_count, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, KOZDEV_CHAN_HW_VER,          1,                       IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KOZDEV_CHAN_SW_VER,          1,                       IS_AUTOUPDATED_TRUSTED);
    SetChanQuant     (devid, KOZDEV_CHAN_OUT_n_base,      countof(me->out), (CxAnyVal_t){.i32=THE_QUANT}, CXDTYPE_INT32);
    SetChanQuant     (devid, KOZDEV_CHAN_OUT_CUR_n_base,  countof(me->out), (CxAnyVal_t){.i32=THE_QUANT}, CXDTYPE_INT32);
    SetChanQuant     (devid, DEVSPEC_CHAN_OUT_IMM_n_base, countof(me->out), (CxAnyVal_t){.i32=THE_QUANT}, CXDTYPE_INT32);

    /* Note: times are in MICROseconds */
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_TAB_TIMES,   1,                       1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_TAB_ALL,     1,                       1000000.0, 0.0);
    SetChanRDs       (devid, KOZDEV_CHAN_OUT_TAB_n_base,  countof(me->out),        1000000.0, 0.0);
    SetChanReturnType(devid, KOZDEV_CHAN_OUT_TAB_ERRDESCR,1,                       IS_AUTOUPDATED_TRUSTED);
    ReportTableStatus(devid, NULL);

    return DEVSTATE_OPERATING;
}

static void cac208_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s()", __FUNCTION__);
    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_DIR, 0,
                          NULL, NULL,
                          0, 1, DESC_STOP);
}

static void cac208_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;
  int        l;       // Line #
  int        hw_ver;
  int        sw_ver;

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, NULL);
    me->ch_cfg[KOZDEV_CHAN_HW_VER] = hw_ver; ReturnCtlCh(me, KOZDEV_CHAN_HW_VER);
    me->ch_cfg[KOZDEV_CHAN_SW_VER] = sw_ver; ReturnCtlCh(me, KOZDEV_CHAN_SW_VER);

    me->supports_unicast_t_ctl = sw_ver >= 4;

    if (ADC_MODE_IS_NORM()) SendMULTICHAN(me);
    else {/*!!!*/}

    if (is_a_reset)
    {
        for (l = 0;  l < countof(me->out);  l++) me->out[l].rcv = 0;
        DoSoftReset(me, 0);
    }
}

static void cac208_alv(int devid, void *devptr,
                       sl_tid_t tid  __attribute__((unused)),
                       void *privptr __attribute__((unused)))
{
  privrec_t  *me     = (privrec_t *) devptr;

    sl_enq_tout_after(devid, devptr, ALIVE_USECS,     cac208_alv, NULL);
    
    if (me->rd_rcvd == 0)
        me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT,
                          1, CANKOZ_DESC_GETDEVATTRS);
    
    me->rd_rcvd = 0;
}

static void cac208_hbt(int devid, void *devptr,
                       sl_tid_t tid  __attribute__((unused)),
                       void *privptr __attribute__((unused)))
{
  privrec_t  *me     = (privrec_t *) devptr;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, cac208_hbt, NULL);

    /*  */
    if (me->num_isc > 0)
        HandleSlowmoHbt(me);

    /* Request current values of table-driven channels, plus periodically ask for DAC state if table is present */
    HandleTableHbt(me);
}

static void cac208_in (int devid, void *devptr,
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
            if (l < 0  ||  l >= CAC208_CHAN_ADC_n_count) return;
            ReturnInt32Datum(devid, KOZDEV_CHAN_ADC_n_base + l,
                             val, rflags);
            break;

        case DESC_RD_DAC_n_BASE ... DESC_RD_DAC_n_BASE + CAC208_CHAN_OUT_n_count-1:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 3) return;

            l = desc - DESC_RD_DAC_n_BASE;
            code   = data[1]*256 + data[2];
            val    = kozdac_c16_to_val(code);
            DoDriverLog(devid, 0 | DRIVERLOG_C_PKTINFO,
                        "in: WCHAN=%d code=0x%04X val=%duv",
                        l, code, val);
            HandleSlowmoREADDAC_in(me, l, val);
            break;

        case CANKOZ_DESC_GETDEVSTAT:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            break;
            
        case DESC_FILE_CLOSE:
            HandleDESC_FILE_CLOSE  (me, desc, dlc, data);
            break;

        case DESC_GET_DAC_STAT:
            HandleDESC_GET_DAC_STAT(me, desc, dlc, data);
            break;

        default:
            ////if (desc != 0xF8) DoDriverLog(devid, 0, "desc=%02x len=%d", desc, datasize);
            ;
    }
}

static void cac208_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int        l;     // Line #
  int32      val;   // Value -- in volts

    for (n = 0;  n < count;  n++)
    {
        /* A. Prepare */
        chn = addrs[n];
        if (action == DRVA_WRITE  &&
            (chn == KOZDEV_CHAN_OUT_TAB_TIMES  ||  chn == KOZDEV_CHAN_OUT_TAB_ALL
             ||
             (chn >= KOZDEV_CHAN_OUT_TAB_n_base  &&
              chn <  KOZDEV_CHAN_OUT_TAB_n_base + DEVSPEC_CHAN_OUT_n_count)
            )
           )
        {
            if ((dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = 0xFFFFFFFF; // That's just to make GCC happy
        }
        else if (action == DRVA_WRITE) /* Classic driver's dtype-check */
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        /* B. Select */
        if      (chn >= KOZDEV_CHAN_REGS_base  &&  chn <= KOZDEV_CHAN_REGS_last)
        {
            me->lvmt->regs_rw(me->handle, action, chn, &val);
        }
        else if (chn >= KOZDEV_CHAN_ADC_n_base  &&
                 chn <  KOZDEV_CHAN_ADC_n_base + CAC208_CHAN_ADC_n_count)
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
        else if (chn == KOZDEV_CHAN_ADC_BEG)
        {
            if (ADC_MODE_IS_NORM())
            {
                if (val < 0)
                    val = 0;
                if (val > CAC208_CHAN_ADC_n_count - 1)
                    val = CAC208_CHAN_ADC_n_count - 1;
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
                if (val > CAC208_CHAN_ADC_n_count - 1)
                    val = CAC208_CHAN_ADC_n_count - 1;
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
        else if (chn >= KOZDEV_CHAN_OUT_n_base       &&
                 chn <  KOZDEV_CHAN_OUT_n_base      + DEVSPEC_CHAN_OUT_n_count)
            HandleSlowmoOUT_rw     (me, chn, action, action == DRVA_WRITE? val : 0);
        else if (chn >= KOZDEV_CHAN_OUT_RATE_n_base  &&
                 chn <  KOZDEV_CHAN_OUT_RATE_n_base + DEVSPEC_CHAN_OUT_n_count)
            HandleSlowmoOUT_RATE_rw(me, chn, action, action == DRVA_WRITE? val : 0);
        else if (chn >= DEVSPEC_CHAN_OUT_IMM_n_base  &&
                 chn <  DEVSPEC_CHAN_OUT_IMM_n_base + DEVSPEC_CHAN_OUT_n_count)
            HandleSlowmoOUT_IMM_rw (me, chn, action, action == DRVA_WRITE? val : 0);
        else if (chn >= DEVSPEC_CHAN_OUT_TAC_n_base  &&
                 chn <  DEVSPEC_CHAN_OUT_TAC_n_base + DEVSPEC_CHAN_OUT_n_count)
            HandleSlowmoOUT_TAC_rw (me, chn, action, action == DRVA_WRITE? val : 0);
        else if (chn >= KOZDEV_CHAN_OUT_CUR_n_base   &&
                 chn <  KOZDEV_CHAN_OUT_CUR_n_base  + DEVSPEC_CHAN_OUT_n_count)
        {
            l = chn - KOZDEV_CHAN_OUT_CUR_n_base;
            if (me->out[l].rcv)
                ReturnInt32Datum(devid, chn, me->out[l].cur, 0);
        }
        else if (chn == CHAN_CURSPD  ||  chn == CHAN_CURACC) ;
        else if ((chn >= KOZDEV_CHAN_DO_TAB_base  &&
                  chn <  KOZDEV_CHAN_DO_TAB_base + KOZDEV_CHAN_DO_TAB_cnt)  ||
                 chn == KOZDEV_CHAN_OUT_TAB_ID     ||
                 chn == KOZDEV_CHAN_OUT_TAB_TIMES  ||
                 chn == KOZDEV_CHAN_OUT_TAB_ALL    ||
                 (chn >= KOZDEV_CHAN_OUT_TAB_n_base  &&
                  chn <  KOZDEV_CHAN_OUT_TAB_n_base + DEVSPEC_CHAN_OUT_n_count) ||
                 chn == KOZDEV_CHAN_OUT_TAB_ERRDESCR)
            HandleTable_rw(me, action, chn,
                           n, dtypes, nelems, values);
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

static void CrunchTableIntoFile(privrec_t *me)
{
  uint8  *wp;
  int     tr;
  int     nsteps;
  int     l;
  /* Note: all these MUST be unsigned, otherwise less-than comparison doesn't work as expected */
  uint32  this_targ;
  uint32  prev_code;
  uint32  delta;
  uint32  this_incr;
  uint32  accums[DEVSPEC_CHAN_OUT_n_count];

    for (tr = 0, wp = me->t_file;  tr < me->t_times_nsteps;  tr++)
        if (tr == 0)
        {
            for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
            {
                if      (me->out[l].aff == 0)
                    accums[l] = 0;
                else if (me->t_vals[l][0] >= DEVSPEC_TABLE_VAL_START_FROM_CUR)
                    accums[l] = (kozdac_val_to_c16(me->out[l].cur))   << 16;
                else
                    accums[l] = (kozdac_val_to_c16(me->t_vals[l][0])) << 16;
            }
        }
        else
        {
            nsteps = me->t_times[tr] / DEVSPEC_TABLE_USECS_IN_STEP;
            if (nsteps < 1) nsteps = 1;
            *wp++ =  nsteps       & 0xFF;
            *wp++ = (nsteps >> 8) & 0xFF;

            for (l = 0;  l < DEVSPEC_CHAN_OUT_n_count;  l++)
            {
                if (me->out[l].aff == 0)
                    this_incr = 0;
                else
                {
                    this_targ = (kozdac_val_to_c16(me->t_vals[l][tr])) << 16;
                    prev_code = accums[l];

                    /* We handle positive and negative changes separately,
                       in order to perform "direct/linear encoding" arithmetics correctly */

                    if (prev_code < this_targ)
                    {
                        delta = this_targ - prev_code;
                        this_incr = delta / nsteps;
                        accums[l] = prev_code + this_incr * nsteps;
                    }
                    else
                    {
                        delta = prev_code - this_targ;
                        this_incr = -(delta / nsteps);
                        accums[l] = prev_code + this_incr * nsteps;
                    }
                }
////fprintf(stderr, "step %d: %04x->%04x in %d cs, delta=%04x incr=%08x\n", tr, prev_code, this_targ, nsteps, delta, this_incr);
                *wp++ =  this_incr        & 0xFF;
                *wp++ = (this_incr >>  8) & 0xFF;
                *wp++ = (this_incr >> 16) & 0xFF;
                *wp++ = (this_incr >> 24) & 0xFF;
            }
        }

    me->t_file_bytes = wp - me->t_file;
}


DEFINE_CXSD_DRIVER(cac208, "CAC208/CEAC208 CAN-DAC/ADC driver",
                   NULL, NULL,
                   sizeof(privrec_t), cac208_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   cac208_init_d, cac208_term_d, cac208_rw_p);

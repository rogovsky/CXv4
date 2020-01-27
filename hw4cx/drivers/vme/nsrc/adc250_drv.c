#include "cxsd_driver.h"
#include "pzframe_drv.h"
#include "vme_lyr.h"

#include "drv_i/adc250_drv_i.h"


//////////////////////////////////////////////////////////////////////
#include "adc4x250_defs.h"

typedef int16 ADC250_DATUM_T;
enum { ADC250_DTYPE = CXDTYPE_INT16};

/* We allow only as much measurements, not ADC250_MAX_NUMPTS  */
enum { ADC250_MAX_ALLOWED_NUMPTS = 50000*1 + ADC250_MAX_NUMPTS*0 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC250_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC250_CHAN_LINE0]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC250_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC250_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC250_CHAN_LINE3]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC250_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC250_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC250_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC250_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC250_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC250_CHAN_CALIBRATE]     = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
    [ADC250_CHAN_FGT_CLB]       = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
    [ADC250_CHAN_VISIBLE_CLB]   = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC250_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_FRQDIV]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_RANGE0]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_RANGE1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_RANGE2]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_RANGE3]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_TRIG_TYPE]     = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC250_CHAN_TRIG_INPUT]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC250_CHAN_DEVICE_ID]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_BASE_SW_VER]   = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_PGA_SW_VER]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_BASE_HW_VER]   = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_PGA_HW_VER]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_PGA_VAR]       = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_BASE_UNIQ_ID]  = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_PGA_UNIQ_ID]   = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC250_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC250_CHAN_CLB_STATE]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_PTSOFS},
    [ADC250_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_NUMPTS},
    [ADC250_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_TIMING},
    [ADC250_CHAN_CUR_FRQDIV]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_FRQDIV},
    [ADC250_CHAN_CUR_RANGE0]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_RANGE0},
    [ADC250_CHAN_CUR_RANGE1]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_RANGE1},
    [ADC250_CHAN_CUR_RANGE2]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_RANGE2},
    [ADC250_CHAN_CUR_RANGE3]    = {PZFRAME_CHTYPE_STATUS,      ADC250_CHAN_RANGE3},
    [ADC250_CHAN_OVERFLOW0]     = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_OVERFLOW1]     = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_OVERFLOW2]     = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_OVERFLOW3]     = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC250_CHAN_LINE0ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE3ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE0RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE3RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE0RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC250_CHAN_LINE3RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC250_CHAN_CLB_FAIL0]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_FAIL1]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_FAIL2]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_FAIL3]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_DYN0]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_DYN1]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_DYN2]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_DYN3]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_ZERO0]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_ZERO1]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_ZERO2]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_ZERO3]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_GAIN0]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_GAIN1]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_GAIN2]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_CLB_GAIN3]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC250_CHAN_MIN0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MIN3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MAX0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_MAX3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_AVG0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_AVG3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_INT0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_INT3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC250_CHAN_LINE0TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE3TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE0TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_LINE3TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC250_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    vme_vmt_t         *lvmt;
    int                devid;
    int                handle;
    int                irq_n;
    int                irq_vect;

    int32              cur_args[ADC250_NUMCHANS];
    int32              nxt_args[ADC250_NUMCHANS];
    int32              prv_args[ADC250_NUMCHANS];
    ADC250_DATUM_T     retdata [ADC250_MAX_ALLOWED_NUMPTS*ADC250_NUM_LINES +1]; // "+1" to safely read one more than requested (odd'th measurement in high-16-bit half of uint32)
    int                do_return;
    int                force_read;
    pzframe_retbufs_t  retbufs;

    // This holds either ADC4X250_CTRL_START or ADC4X250_CTRL_CALIB
    int32              start_mask;

    int                data_rqd;
    int                line_rqd[ADC250_NUM_LINES];
    struct
    {
        int            addrs [ADC250_NUMCHANS];
        cxdtype_t      dtypes[ADC250_NUMCHANS];
        int            nelems[ADC250_NUMCHANS];
        void          *val_ps[ADC250_NUMCHANS];
        rflags_t       rflags[ADC250_NUMCHANS];
    }                  r;
} adc250_privrec_t;

#define PDR2ME(pdr) ((adc250_privrec_t*)pdr) //!!! Should better subtract offsetof(pz)

//--------------------------------------------------------------------

static psp_paramdescr_t adc250_params[] =
{
    PSP_P_END()
};

//--------------------------------------------------------------------

static void Return1Param(adc250_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void ReturnDevInfo(adc250_privrec_t *me)
{
  int32  v_devid;
  int32  v_ver;
  int32  v_uniq;

    me->lvmt->a32rd32(me->handle, ADC4X250_R_DEVICE_ID, &v_devid);
    me->lvmt->a32rd32(me->handle, ADC4X250_R_VERSION,   &v_ver);
    me->lvmt->a32rd32(me->handle, ADC4X250_R_UNIQ_ID,   &v_uniq);

    Return1Param(me, ADC250_CHAN_DEVICE_ID,     v_devid);

    Return1Param(me, ADC250_CHAN_BASE_SW_VER,   v_ver         & 0xFF);
    Return1Param(me, ADC250_CHAN_PGA_SW_VER,   (v_ver  >>  8) & 0xFF);
    Return1Param(me, ADC250_CHAN_BASE_HW_VER,  (v_ver  >> 16) & 0xFF);
    Return1Param(me, ADC250_CHAN_PGA_HW_VER,   (v_ver  >> 24) & 0x3F);
    Return1Param(me, ADC250_CHAN_PGA_VAR,      (v_ver  >> 30) & 0x03);

    Return1Param(me, ADC250_CHAN_BASE_UNIQ_ID,  v_uniq        & 0xFFFF);
    Return1Param(me, ADC250_CHAN_PGA_UNIQ_ID,  (v_uniq >> 16) & 0xFFFF);
}

static void ReturnClbInfo(adc250_privrec_t *me)
{
  uint32  status;
  int32   iv;
  int     n;

    me->lvmt->a32rd32(me->handle, ADC4X250_R_STATUS, &status);
    for (n = 0;  n < ADC250_NUM_ADCS;  n++)
    {
        Return1Param(me, ADC250_CHAN_CLB_FAIL0 + n,
                     (status >> (ADC4X250_STATUS_CALIB_FAILED_shift + n)) & 1);

        me->lvmt->a32rd32(me->handle,
                          ADC4X250_R_CALIB_CONST_DYN_CHx_base  +
                          ADC4X250_R_CALIB_CONST_DYN_CHx_incr  * n,
                          &iv);
        Return1Param(me, ADC250_CHAN_CLB_DYN0  + n, iv);

        me->lvmt->a32rd32(me->handle,
                          ADC4X250_R_CALIB_CONST_ZERO_CHx_base +
                          ADC4X250_R_CALIB_CONST_ZERO_CHx_incr * n,
                          &iv);
        Return1Param(me, ADC250_CHAN_CLB_ZERO0 + n, iv);

        me->lvmt->a32rd32(me->handle,
                          ADC4X250_R_CALIB_CONST_GAIN_CHx_base +
                          ADC4X250_R_CALIB_CONST_GAIN_CHx_incr * n,
                          &iv);
        Return1Param(me, ADC250_CHAN_CLB_GAIN0 + n, iv);
    }
}

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int32 v)
{
  adc250_privrec_t *me = PDR2ME(pdr);

    if (0)
    ;
    else if (n == ADC250_CHAN_TIMING)
    {
        if (v != ADC4X250_CLK_SRC_VAL_INT  &&
            v != ADC4X250_CLK_SRC_VAL_FP   &&
            v != ADC4X250_CLK_SRC_VAL_BP) v = ADC4X250_CLK_SRC_VAL_INT;
    }
    else if (n == ADC250_CHAN_FRQDIV)
    {
        /*???*/
    }
    else if (n >= ADC250_CHAN_RANGE0  &&  n <= ADC250_CHAN_RANGE3)
    {
        if (v < ADC4X250_RANGE_VAL_0_5V) v = ADC4X250_RANGE_VAL_0_5V;
        if (v > ADC4X250_RANGE_VAL_4V)   v = ADC4X250_RANGE_VAL_4V;
    }
    else if (n == ADC250_CHAN_TRIG_TYPE)
    {
        if (v != ADC4X250_TRIG_ENA_VAL_DISABLE  &&
            v != ADC4X250_TRIG_ENA_VAL_INT      &&
            v != ADC4X250_TRIG_ENA_VAL_EXT      &&
            v != ADC4X250_TRIG_ENA_VAL_BP       &&
            v != ADC4X250_TRIG_ENA_VAL_BP_SYNC) v = ADC4X250_TRIG_ENA_VAL_DISABLE;
    }
    else if (n == ADC250_CHAN_TRIG_INPUT)
    {
        if (v < 0) v = 0;
        if (v > ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_bits)
            v = ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_bits;
    }
    else if (n == ADC250_CHAN_PTSOFS)
    {
        if (v < 0)                           v = 0;
        if (v > ADC250_MAX_ALLOWED_NUMPTS-1) v = ADC250_MAX_ALLOWED_NUMPTS-1;
    }
    else if (n == ADC250_CHAN_NUMPTS)
    {
        if (v < 1)                           v = 1;
        if (v > ADC250_MAX_ALLOWED_NUMPTS)   v = ADC250_MAX_ALLOWED_NUMPTS;
    }

    return v;
}

enum
{
    HEARTBEAT_FREQ  = 10,
    HEARTBEAT_USECS = 1000000 / HEARTBEAT_FREQ,
};
static void adc250_hbt(int devid, void *devptr,
                       sl_tid_t tid  __attribute__((unused)),
                       void *privptr __attribute__((unused)));
static void  Init1Param(adc250_privrec_t *me, int n, int32 v)
{
    /*if (me->nxt_args[n] < 0)*/ me->nxt_args[n] = v;
    me->nxt_args[n] = ValidateParam(&(me->pz), n, me->nxt_args[n]);
}
static int   InitParams(pzframe_drv_t *pdr)
{
  adc250_privrec_t *me = PDR2ME(pdr);

  int               n;
  uint32            w;

/*
    Note: to stop the device, do
          ADC4X250_R_INT_ENA:=0
          ADC4X250_R_CTRL:=ADC4X250_CTRL_ADC_BREAK_ACK
*/

    /*!!! Stop device */
    me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL,       ADC4X250_CTRL_ADC_BREAK_ACK); // Stop (or drop ADC_CMPLT)
    me->lvmt->a32rd32(me->handle, ADC4X250_R_INT_STATUS, &w); // Drop IRQ

    /*!!! Program IRQ? */
    if (me->irq_n != 5)
        DoDriverLog(me->devid, 0, "WARNING: irq_n=%d, but ADC4x250 supports only =5", me->irq_n);
    me->lvmt->a32wr32(me->handle, ADC4X250_R_INT_LINE,   me->irq_n);
    me->lvmt->a32wr32(me->handle, ADC4X250_R_INT_VECTOR, me->irq_vect);
    me->lvmt->a32wr32(me->handle, ADC4X250_R_INT_ENA,
                      ADC4X250_INT_ENA_ALL       |
                      ADC4X250_INT_ENA_ADC_CMPLT |
                      ADC4X250_INT_ENA_CALIB_CMPLT);

    ReturnDevInfo(me);
    ReturnClbInfo(me);

    /* Read current values from device */
    me->lvmt->a32rd32(me->handle, ADC4X250_R_TIMER,             &w);
fprintf(stderr, "%s *** %s ***\n", strcurtime_msc(), __FUNCTION__);
fprintf(stderr, "TIMER=%08x\n", w);
//    if (w > ADC4X250_TIMER_PRETRIG) w -= ADC4X250_TIMER_PRETRIG;
    if (w <= 1)                        w = 1024;
    if (w > ADC250_MAX_ALLOWED_NUMPTS) w = ADC250_MAX_ALLOWED_NUMPTS;
    Init1Param(me, ADC250_CHAN_NUMPTS, w);
    Init1Param(me, ADC250_CHAN_PTSOFS, 0);

    me->lvmt->a32rd32(me->handle, ADC4X250_R_CLK_SRC,           &w);
    Init1Param(me, ADC250_CHAN_TIMING,
               (w >> ADC4X250_CLK_SRC_shift) & ADC4X250_CLK_SRC_bits);

    me->lvmt->a32rd32(me->handle, ADC4X250_R_ADC_TRIG_SOURCE,   &w);
fprintf(stderr, "TRIG_SOURCE=%08x\n", w);
    Init1Param(me, ADC250_CHAN_TRIG_TYPE, 
               (w >> ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_shift)
                   & ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_bits);
    Init1Param(me, ADC250_CHAN_TRIG_INPUT,
               (w >> ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_shift)
                   & ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_bits);
fprintf(stderr, "[ADC250_CHAN_TRIG_TYPE]=%d [ADC250_CHAN_TRIG_INPUT]=%d\n", me->nxt_args[ADC250_CHAN_TRIG_TYPE], me->nxt_args[ADC250_CHAN_TRIG_INPUT]);

    for (n = 0;  n < ADC250_NUM_LINES;  n++)
    {
        me->lvmt->a32rd32(me->handle,
                          ADC4X250_R_PGA_RANGE_CHx_base + n * ADC4X250_R_PGA_RANGE_CHx_incr,
                          &w);
        Init1Param(me, ADC250_CHAN_RANGE0 + n,
                   (w >> ADC4X250_PGA_RANGE_shift) & ADC4X250_PGA_RANGE_bits);
    }

    /* Device properties */
    Return1Param(me, ADC250_CHAN_XS_FACTOR,  -9);
    for (n = 0;  n < ADC250_NUM_LINES;  n++)
    {
        Return1Param(me, ADC250_CHAN_LINE0TOTALMIN + n, ADC250_MIN_VALUE);
        Return1Param(me, ADC250_CHAN_LINE0TOTALMAX + n, ADC250_MAX_VALUE);
    }
    Return1Param    (me, ADC250_CHAN_NUM_LINES,         ADC250_NUM_LINES);

    SetChanRDs(me->devid, ADC250_CHAN_DATA, 1+ADC250_NUM_LINES, 4096.0, 0.0);
    SetChanRDs(me->devid, ADC250_CHAN_STATS_first,
                          ADC250_CHAN_STATS_last-ADC250_CHAN_STATS_first+1,
                                                                4096.0, 0.0);
    SetChanRDs(me->devid, ADC250_CHAN_ELAPSED, 1, 1000.0, 0.0);

#if 1
    sl_enq_tout_after(me->devid, me, HEARTBEAT_USECS, adc250_hbt, NULL);
#endif

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc250_privrec_t *me = PDR2ME(pdr);

  int               nl;
  uint32            w;

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Zeroth step: check PTSOFS against NUMPTS */
    if (me->cur_args    [ADC250_CHAN_PTSOFS] > ADC250_MAX_NUMPTS - me->cur_args[ADC250_CHAN_NUMPTS])
        Return1Param(me, ADC250_CHAN_PTSOFS,   ADC250_MAX_NUMPTS - me->cur_args[ADC250_CHAN_NUMPTS]);

    /* Check if calibration-affecting parameters had changed */
    if (0)
        /*!!! InvalidateCalibrations(me) */;

    /* Store parameters for future comparison */
    memcpy(me->prv_args, me->cur_args, sizeof(me->prv_args));

    /* a. Prepare: stop/init the device */
    me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL,       ADC4X250_CTRL_ADC_BREAK_ACK); // Stop
    me->lvmt->a32rd32(me->handle, ADC4X250_R_INT_STATUS, &w); // Drop IRQ
    /* b. Set parameters */
    me->lvmt->a32wr32(me->handle, ADC4X250_R_TIMER,
                      me->cur_args[ADC250_CHAN_PTSOFS] +
                      me->cur_args[ADC250_CHAN_NUMPTS]/* + ADC4X250_TIMER_PRETRIG*/);
    me->lvmt->a32wr32(me->handle, ADC4X250_R_CLK_SRC, me->cur_args[ADC250_CHAN_TIMING]);
    /*!!! FRQDIV: PLL registers... */
    for (nl = 0;  nl < ADC250_NUM_LINES;  nl++)
    {
        me->lvmt->a32wr32(me->handle,
                          ADC4X250_R_PGA_RANGE_CHx_base + nl * ADC4X250_R_PGA_RANGE_CHx_incr,
                          me->cur_args[ADC250_CHAN_RANGE0 + nl]);
    }
    me->lvmt->a32wr32(me->handle, ADC4X250_R_ADC_TRIG_SOURCE,
                      ((me->cur_args[ADC250_CHAN_TRIG_TYPE]  & ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_bits)  << ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_shift)
                      |
                      ((me->cur_args[ADC250_CHAN_TRIG_INPUT] & ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_bits) << ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_shift));

    /* Calibration-related... */
    /* a. Decide */
    if ((me->cur_args[ADC250_CHAN_CALIBRATE] & CX_VALUE_LIT_MASK) != 0)
    {
        me->start_mask = ADC4X250_CTRL_CALIB;
        Return1Param(me, ADC250_CHAN_CALIBRATE, 0);
    }
    else
        me->start_mask = ADC4X250_CTRL_START;
    /* b. Related activity */
    me->do_return =
        (me->start_mask != ADC4X250_CTRL_CALIB  ||
         (me->cur_args[ADC250_CHAN_VISIBLE_CLB] & CX_VALUE_LIT_MASK) != 0);
    me->force_read = me->start_mask == ADC4X250_CTRL_CALIB;

    /* Let's go! */
    SleepBySelect(10000);
    me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL, me->start_mask);

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc250_privrec_t *me = PDR2ME(pdr);

    /* Don't even try anything in CALIBRATE mode */
    if (me->start_mask == ADC4X250_CTRL_CALIB) return PZFRAME_R_DOING;

    /* 0. Switch to prog-start mode */
    me->lvmt->a32wr32(me->handle, ADC4X250_R_ADC_TRIG_SOURCE,
                      (ADC4X250_TRIG_ENA_VAL_INT            << ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_shift)
                      |
                      (me->cur_args[ADC250_CHAN_TRIG_INPUT] << ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_shift));

    /* 1. Do prog-start */
    me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL, ADC4X250_CTRL_START);

    /* 2. Revert to what-programmed start mode */
    me->lvmt->a32wr32(me->handle, ADC4X250_R_ADC_TRIG_SOURCE,
                      ((me->cur_args[ADC250_CHAN_TRIG_TYPE]  & ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_bits)  << ADC4X250_ADC_TRIG_SOURCE_TRIG_ENA_shift)
                      |
                      ((me->cur_args[ADC250_CHAN_TRIG_INPUT] & ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_bits) << ADC4X250_ADC_TRIG_SOURCE_TTL_INPUT_shift));

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc250_privrec_t *me = PDR2ME(pdr);
  uint32            w;
  int               n;

    me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL,       ADC4X250_CTRL_ADC_BREAK_ACK); // Stop
    me->lvmt->a32rd32(me->handle, ADC4X250_R_INT_STATUS, &w); // Drop IRQ

    if (me->cur_args[ADC250_CHAN_NUMPTS] != 0)
        bzero(me->retdata,
              sizeof(me->retdata[0])
              *
              me->cur_args[ADC250_CHAN_NUMPTS] * ADC250_NUM_LINES);

    for (n = ADC250_CHAN_STATS_first;  n <= ADC250_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc250_privrec_t *me = PDR2ME(pdr);

  int               numduplets;
  int32             w; // Note SIGNEDNESS
  uint32            status;

  int               nl;
  uint32            ro; // Read Offset
  ADC250_DATUM_T   *dp;
  int               n;

////fprintf(stderr, "%s %s ENTRY\n", strcurtime_msc(), __FUNCTION__);
    /* Was it a calibration? */
    if (me->start_mask == ADC4X250_CTRL_CALIB)
        ;

    /* Stop the device */
    me->lvmt->a32rd32(me->handle, ADC4X250_R_INT_STATUS, &w);
////fprintf(stderr, "INT_STATUS=%08x\n", w);
    me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL,       ADC4X250_CTRL_ADC_BREAK_ACK); // Stop

    numduplets = (me->cur_args[ADC250_CHAN_NUMPTS] + 1) / 2;
    for (nl = 0;  nl < ADC250_NUM_LINES;  nl++)
    {
        dp = me->retdata + nl * me->cur_args[ADC250_CHAN_NUMPTS];
        ro = ADC4X250_DATA_ADDR_CHx_base + nl * ADC4X250_DATA_ADDR_CHx_incr;
        for (n = numduplets;  n > 0;  n--)
        {
            me->lvmt->a32rd32(me->handle, ro, &w);
            *dp++ =  w        & 0xFFFF;
            *dp++ = (w >> 16) & 0xFFFF;
        }
    }
    for (n = ADC250_CHAN_STATS_first;  n <= ADC250_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

/*!!! ADC250_CHAN_CLB_STATE, ADC250_CHAN_CLB_FAILx, ADC250_CHAN_OVERFLOWx */
    me->lvmt->a32rd32(me->handle, ADC4X250_R_STATUS, &status);
    for (n = 0;  n < ADC250_NUM_ADCS;  n++)
        me->cur_args[ADC250_CHAN_OVERFLOW0 + n] =
        me->nxt_args[ADC250_CHAN_OVERFLOW0 + n] =
            (status >> (ADC4X250_STATUS_PGA_OVERRNG_shift + n)) & 1;

////fprintf(stderr, "%s EXIT\n", __FUNCTION__);
    return 0;
}

static void adc250_hbt(int devid, void *devptr,
                       sl_tid_t tid  __attribute__((unused)),
                       void *privptr __attribute__((unused)))
{
  adc250_privrec_t  *me     = (adc250_privrec_t *) devptr;
  uint32             w;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, adc250_hbt, NULL);

return;
    me->lvmt->a32rd32(me->handle, ADC4X250_R_INT_STATUS, &w);
    if (w == 0) return;
fprintf(stderr, "%s %s(): INT_STATUS=%08x, simulating IRQ\n", strcurtime_msc(), __FUNCTION__, w);

    if (me->do_return == 0  &&  me->force_read) ReadMeasurements(&(me->pz));
    pzframe_drv_drdy_p(&(me->pz), me->do_return, 0);
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc250_privrec_t *me = PDR2ME(pdr);

  int               n;
  int               x;

////fprintf(stderr, "%s %s ENTRY\n", strcurtime_msc(), __FUNCTION__);
    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[ADC250_CHAN_XS_PER_POINT] =
4;
//        1000 * timing_scales[me->cur_args[ADC250_CHAN_TIMING] & 1];
    me->cur_args[ADC250_CHAN_XS_DIVISOR]   =
1;
//        1000 / frqdiv_scales[me->cur_args[ADC250_CHAN_FRQDIV] & 3];
    for (x = 0;  x < ADC250_NUM_LINES;  x++)
    {
        me->cur_args[ADC250_CHAN_LINE0ON       + x] = 1;
        me->cur_args[ADC250_CHAN_LINE0RANGEMIN + x] =
ADC250_MIN_VALUE;
//            (    0-2048) * quants[me->cur_args[ADC250_CHAN_RANGE0 + x] & 3];
        me->cur_args[ADC250_CHAN_LINE0RANGEMAX + x] =
ADC250_MAX_VALUE;
//            (0xFFF-2048) * quants[me->cur_args[ADC250_CHAN_RANGE0 + x] & 3];
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC250_CHAN_CALC_STATS])
        for (x = ADC250_CHAN_STATS_first;  x <= ADC250_CHAN_STATS_last; x++)
        {
            me->r.addrs [n] = x;
            me->r.dtypes[n] = CXDTYPE_INT32;
            me->r.nelems[n] = 1;
            me->r.val_ps[n] = me->cur_args + x;
            me->r.rflags[n] = 0;
            n++;
        }

    /* 3. General */

    /* Scalar STATUS channels */
    for (x = 0;  x < countof(chinfo);  x++)
        if (chinfo[x].chtype == PZFRAME_CHTYPE_STATUS)
        {
            if (chinfo[x].refchn >= 0) // Copy CUR value from respective control one
                me->cur_args[x] = me->cur_args[chinfo[x].refchn];
            me->nxt_args[x] = me->cur_args[x]; // For them to stay in place upon copy cur_args[]=nxt_args[]
            me->r.addrs [n] = x;
            me->r.dtypes[n] = CXDTYPE_INT32;
            me->r.nelems[n] = 1;
            me->r.val_ps[n] = me->cur_args + x;
            me->r.rflags[n] = 0;
            n++;
        }

    /* Vector channels, which were requested */
    if (me->data_rqd)
    {
        me->r.addrs [n] = ADC250_CHAN_DATA;
        me->r.dtypes[n] = ADC250_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC250_CHAN_NUMPTS] * ADC250_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0;  x < ADC250_NUM_LINES;  x++)
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC250_CHAN_LINE0 + x;
            me->r.dtypes[n] = ADC250_DTYPE;
            me->r.nelems[n] = me->cur_args[ADC250_CHAN_NUMPTS];
            me->r.val_ps[n] = me->retdata + me->cur_args[ADC250_CHAN_NUMPTS] * x;
            me->r.rflags[n] = add_rflags;
            n++;
        }
    /* A marker */
    me->r.addrs [n] = ADC250_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC250_CHAN_MARKER;
    me->r.rflags[n] = add_rflags;
    n++;
    /* ...and drop "requested" flags */
    me->data_rqd = 0;
    bzero(me->line_rqd, sizeof(me->line_rqd));

    /* Store retbufs data */
    me->pz.retbufs.count      = n;
    me->pz.retbufs.addrs      = me->r.addrs;
    me->pz.retbufs.dtypes     = me->r.dtypes;
    me->pz.retbufs.nelems     = me->r.nelems;
    me->pz.retbufs.values     = me->r.val_ps;
    me->pz.retbufs.rflags     = me->r.rflags;
    me->pz.retbufs.timestamps = NULL;

#if 0
fprintf(stderr, "retbufs.count=%d\n", n);
for (x = 0;  x < me->pz.retbufs.count;  x++)
    fprintf(stderr, "[%d] #%d dt=%d nel=%d\n", x, me->pz.retbufs.addrs[x], me->pz.retbufs.dtypes[x], me->pz.retbufs.nelems[x]);
#endif
////fprintf(stderr, "%s EXIT\n", __FUNCTION__);
}

static void adc250_rw_p (int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  adc250_privrec_t *me = devptr;

  int               n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int               chn;   // channel
  int               ct;
  int32             val;   // Value

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if (chn < 0  ||  chn >= countof(chinfo)  ||
            (ct = chinfo[chn].chtype) == PZFRAME_CHTYPE_UNSUPPORTED)
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else if (ct == PZFRAME_CHTYPE_BIGC)
        {
////fprintf(stderr, "%s chn=%d action=%d\n", strcurtime_msc(), chn, action);
            if (chn == ADC250_CHAN_DATA)
                me->data_rqd                          = 1;
            else
                me->line_rqd[chn-ADC250_CHAN_LINE0] = 1;

            pzframe_drv_req_mes(&(me->pz));
        }
        else if (ct == PZFRAME_CHTYPE_MARKER)
        {/* Ignore */}
        else
        {
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

            if      (ct == PZFRAME_CHTYPE_STATUS  ||  ct == PZFRAME_CHTYPE_AUTOUPDATED)
            {
                ////if (chn == ADC250_CHAN_CUR_NUMPTS) fprintf(stderr, "ADC250_CHAN_CUR_NUMPTS. =%d\n", me->cur_args[chn]);
                ReturnInt32Datum(devid, chn, me->cur_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_VALIDATE)
            {
                if (action == DRVA_WRITE)
                    me->nxt_args[chn] = ValidateParam(&(me->pz), chn, val);
                ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_INDIVIDUAL)
            {
                if      (chn == ADC250_CHAN_CALIBRATE)
                {
                     if (action == DRVA_WRITE)
                          me->nxt_args[chn] = (val != 0);
                     ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
                }
                else if (chn == ADC250_CHAN_FGT_CLB)
                {
                    if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                        /*!!! InvalidateCalibrations(me) */;
                    ReturnInt32Datum(devid, chn, 0, 0);
                }
            }
            else /*  ct == PZFRAME_CHTYPE_PZFRAME_STD, which also returns UPSUPPORTED for unknown */
                ////fprintf(stderr, "PZFRAME_STD(%d,%d)\n", chn, val),
                pzframe_drv_rw_p  (&(me->pz), chn,
                                   action == DRVA_WRITE? val : 0,
                                   action);
        }

 NEXT_CHANNEL:;
    }
}


enum
{
    PARAM_SHOT     = ADC250_CHAN_SHOT,
    PARAM_ISTART   = ADC250_CHAN_ISTART,
    PARAM_WAITTIME = ADC250_CHAN_WAITTIME,
    PARAM_STOP     = ADC250_CHAN_STOP,
    PARAM_ELAPSED  = ADC250_CHAN_ELAPSED,
};

enum
{
    FASTADC_SPACE_SIZE       = 0xB80000,
    FASTADC_ADDRESS_MODIFIER = ADC4X250_ADDRESS_MODIFIER,
};

#define FASTADC_NAME adc250
//#include "vme_fastadc_common.h"
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
                           int irq_n, int irq_vect)
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;
  uint32               w;

fprintf(stderr, "%s IRQ[%d]: n=%d vector=%d\n", strcurtime_msc(), devid, irq_n, irq_vect);
    if (irq_vect != me->irq_vect) return;
fprintf(stderr, "Yes!\n");
me->lvmt->a32rd32(me->handle, ADC4X250_R_INT_STATUS, &w);
fprintf(stderr, "zINT_STATUS=%08x\n", w);
me->lvmt->a32wr32(me->handle, ADC4X250_R_CTRL,       ADC4X250_CTRL_ADC_BREAK_ACK); // Stop (or drop ADC_CMPLT)

    /*!!! Here MUST call smth to read ADC4X250_R_INT_STATUS (thus dropping IRQ) */
    if (me->do_return == 0  &&  me->force_read) ReadMeasurements(&(me->pz));
    pzframe_drv_drdy_p(&(me->pz), me->do_return, 0);
}

static int  FASTADC_INIT_D(int devid, void *devptr, 
                           int businfocount, int businfo[],
                           const char *auxinfo __attribute__((unused)))
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;
  int                  n;
  int                  jumpers;

    jumpers      = businfo[0]; /*!!!*/
    me->irq_n    = businfo[1] &  0x7;
    me->irq_vect = businfo[2] & 0xFF;
fprintf(stderr, "%s businfo[0]=%08x jumpers=0x%x irq=%d vector=%d\n", strcurtime_msc(), businfo[0], jumpers, me->irq_n, me->irq_vect);

    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               jumpers << 24, FASTADC_SPACE_SIZE, FASTADC_ADDRESS_MODIFIER,
                               me->irq_n, me->irq_vect, FASTADC_IRQ_P);
    if (me->handle < 0) return me->handle;

    for (n = 0;  n < countof(chinfo);  n++)
    {
        if      (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
                 chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);
        else if (chinfo[n].chtype == PZFRAME_CHTYPE_BIGC)
            SetChanReturnType(me->devid, n, 1, DO_IGNORE_UPD_CYCLE);
    }

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

    /*!!! Should do something to "stop"!!! */
    /*!!! ...and forbid IRQs!!! */

    pzframe_drv_term(&(me->pz));
}

DEFINE_CXSD_DRIVER(FASTADC_NAME, __CX_STRINGIZE(FASTADC_NAME) " fast-ADC",
                   NULL, NULL,
                   sizeof(FASTADC_PRIVREC_T), FASTADC_PARAMS,
                   3, 3,
                   VME_LYR_NAME, VME_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   FASTADC_INIT_D, FASTADC_TERM_D, FASTADC_RW_P);

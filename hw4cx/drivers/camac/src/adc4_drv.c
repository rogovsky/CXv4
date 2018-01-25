#include "cxsd_driver.h"
#include "pzframe_drv.h"

#include "drv_i/adc4_drv_i.h"


enum
{
    A_ANY  = 0,

    A_ADDR = 0,
    A_STAT = 1,

    STAT_NONE  = 0,       // No bits -- stop
    STAT_AINC  = 1 << 0,  // Enable auto-increment on reads
    STAT_START = 1 << 1,  // Start measurements
    STAT_EXT   = 1 << 2,  // When =1, start on external pulse (START), =0 -- start immediately
    
    ADDR_EOM   = 32767,   // End-Of-Memory
};


typedef int32 ADC4_DATUM_T;
enum { ADC4_DTYPE = CXDTYPE_INT32 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC4_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC4_CHAN_LINE0]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC4_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC4_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC4_CHAN_LINE3]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC4_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC4_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC4_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC4_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC4_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC4_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC4_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC4_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC4_CHAN_ZERO0]         = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC4_CHAN_ZERO1]         = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC4_CHAN_ZERO2]         = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC4_CHAN_ZERO3]         = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC4_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC4_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC4_CHAN_PTSOFS},
    [ADC4_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC4_CHAN_NUMPTS},
    [ADC4_CHAN_CUR_ZERO0]     = {PZFRAME_CHTYPE_STATUS,      ADC4_CHAN_ZERO0},
    [ADC4_CHAN_CUR_ZERO1]     = {PZFRAME_CHTYPE_STATUS,      ADC4_CHAN_ZERO1},
    [ADC4_CHAN_CUR_ZERO2]     = {PZFRAME_CHTYPE_STATUS,      ADC4_CHAN_ZERO2},
    [ADC4_CHAN_CUR_ZERO3]     = {PZFRAME_CHTYPE_STATUS,      ADC4_CHAN_ZERO3},

    [ADC4_CHAN_LINE0ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE3ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE0RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE3RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE0RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC4_CHAN_LINE3RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC4_CHAN_MIN0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MIN3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MAX0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_MAX3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_AVG0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_AVG3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_INT0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_INT3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC4_CHAN_LINE0TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE3TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE0TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_LINE3TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC4_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t    pz;
    int              devid;
    int              N_DEV;

    int32              cur_args[ADC4_NUMCHANS];
    int32              nxt_args[ADC4_NUMCHANS];
    ADC4_DATUM_T       retdata [ADC4_MAX_NUMPTS*ADC4_NUM_LINES];
    pzframe_retbufs_t  retbufs;

    int                data_rqd;
    int                line_rqd[ADC4_NUM_LINES];
    struct
    {
        int            addrs [ADC4_NUMCHANS];
        cxdtype_t      dtypes[ADC4_NUMCHANS];
        int            nelems[ADC4_NUMCHANS];
        void          *val_ps[ADC4_NUMCHANS];
        rflags_t       rflags[ADC4_NUMCHANS];
    }                  r;

#ifdef ADC4_EXT_PRIVREC
    ADC4_EXT_PRIVREC
#endif
} adc4_privrec_t;

#define PDR2ME(pdr) ((adc4_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static psp_paramdescr_t adc4_params[] =
{
    PSP_P_FLAG("calcstats", adc4_privrec_t, nxt_args[ADC4_CHAN_CALC_STATS], 1, 0),

    PSP_P_INT ("numpts", adc4_privrec_t, nxt_args[ADC4_CHAN_NUMPTS], -1, 1, ADC4_MAX_NUMPTS),

    PSP_P_INT ("zero0", adc4_privrec_t, nxt_args[ADC4_CHAN_ZERO0], 0, ADC4_MIN_VALUE, ADC4_MAX_VALUE),
    PSP_P_INT ("zero1", adc4_privrec_t, nxt_args[ADC4_CHAN_ZERO1], 0, ADC4_MIN_VALUE, ADC4_MAX_VALUE),
    PSP_P_INT ("zero2", adc4_privrec_t, nxt_args[ADC4_CHAN_ZERO2], 0, ADC4_MIN_VALUE, ADC4_MAX_VALUE),
    PSP_P_INT ("zero3", adc4_privrec_t, nxt_args[ADC4_CHAN_ZERO3], 0, ADC4_MIN_VALUE, ADC4_MAX_VALUE),
#ifdef ADC4_EXT_PARAMS
    ADC4_EXT_PARAMS
#endif
    PSP_P_END()
};

//--------------------------------------------------------------------

static inline rflags_t adc4_code2uv(uint16 code, int32 zero_shift,
                                    ADC4_DATUM_T *r_p)
{
    *r_p = code - 2048 - zero_shift; /*!!!*/
    return 0;
}

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int32 v)
{
    if      (n == ADC4_CHAN_PTSOFS)
    {
        if (v < 0)                 v = 0;
        if (v > ADC4_MAX_NUMPTS-1) v = ADC4_MAX_NUMPTS-1;
    }
    else if (n == ADC4_CHAN_NUMPTS)
    {
        if (v < 1)                 v = 1;
        if (v > ADC4_MAX_NUMPTS)   v = ADC4_MAX_NUMPTS;
    }
    else if (n >= ADC4_CHAN_ZERO0  &&  n <= ADC4_CHAN_ZERO3)
    {
        if (v < ADC4_MIN_VALUE) v = ADC4_MIN_VALUE;
        if (v > ADC4_MAX_VALUE) v = ADC4_MAX_VALUE;
    }
#ifdef ADC4_EXT_VALIDATE_PARAM
    ADC4_EXT_VALIDATE_PARAM
#endif

    return v;
}

static void Return1Param(adc4_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void Init1Param(adc4_privrec_t *me, int n, int32 v)
{
    if (me->nxt_args[n] < 0) me->nxt_args[n] = v;
}
static int   InitParams(pzframe_drv_t *pdr)
{
  adc4_privrec_t *me = PDR2ME(pdr);

  int             n;

    Init1Param(me, ADC4_CHAN_NUMPTS, 1024);

    Return1Param(me, ADC4_CHAN_XS_DIVISOR, 0);
    Return1Param(me, ADC4_CHAN_XS_FACTOR,  -9);

    for (n = 0;  n < ADC4_NUM_LINES;  n++)
    {
        Return1Param(me, ADC4_CHAN_LINE0TOTALMIN + n, ADC4_MIN_VALUE);
        Return1Param(me, ADC4_CHAN_LINE0TOTALMAX + n, ADC4_MAX_VALUE);
    }
    Return1Param    (me, ADC4_CHAN_NUM_LINES,         ADC4_NUM_LINES);

    /* Note: no RDs for DATA,{1+NUM_LINES} -- because no calibration, just codes */
    SetChanRDs(me->devid, ADC4_CHAN_ELAPSED, 1, 1000.0, 0.0);

#ifdef ADC4_EXT_INIT_PARAMS
    ADC4_EXT_INIT_PARAMS
#endif

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc4_privrec_t *me = PDR2ME(pdr);

  int             w;
    
    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Additional check of PTSOFS against NUMPTS */
    if (me->cur_args    [ADC4_CHAN_PTSOFS] > ADC4_MAX_NUMPTS - me->cur_args[ADC4_CHAN_NUMPTS])
        Return1Param(me, ADC4_CHAN_PTSOFS,   ADC4_MAX_NUMPTS - me->cur_args[ADC4_CHAN_NUMPTS]);

    /* Program the device */
    /* a. Prepare... */
    w = STAT_NONE;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);  // Stop device
    DO_NAF(CAMAC_REF, me->N_DEV, 0,      0,  &w);  // Dummy read -- reset LAM
    /* b. Set parameters */
    w = ADDR_EOM -
        me->cur_args[ADC4_CHAN_NUMPTS] -
        me->cur_args[ADC4_CHAN_PTSOFS] -
        ADC4_JUNK_MSMTS; // 6 junk measurements
    DO_NAF(CAMAC_REF, me->N_DEV, A_ADDR, 16, &w);  // Set addr
#ifdef ADC4_EXT_START
    ADC4_EXT_START
#endif
    /* c. Allow start */
    w = STAT_START | STAT_EXT*1;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc4_privrec_t *me = PDR2ME(pdr);

  int             w;

    w = STAT_START | STAT_EXT*0;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);  // Set EXT=0, thus causing immediate prog-start

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc4_privrec_t *me = PDR2ME(pdr);
  int             n;

  int             w;

    w = STAT_NONE;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);  // Stop device
    DO_NAF(CAMAC_REF, me->N_DEV, 0,      0,  &w);  // Dummy read -- reset LAM
  
    if (me->cur_args[ADC4_CHAN_NUMPTS] != 0)
        bzero(me->retdata,
              sizeof(me->retdata[0])
              *
              me->cur_args[ADC4_CHAN_NUMPTS] * ADC4_NUM_LINES);

    for (n = ADC4_CHAN_STATS_first;  n <= ADC4_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc4_privrec_t *me = PDR2ME(pdr);

  int             w;
  int             N_DEV = me->N_DEV;
  ADC4_DATUM_T   *dp;
  int             nl;
  int32           zero_shift;
  int             n;
  enum           {COUNT_MAX = 100};
  int             count;
  int             x;
  int             rdata[COUNT_MAX];
  int             v;
  int             cum_v;
  ADC4_DATUM_T    vd;
  int32           min_v, max_v, sum_v;
  
    /* Stop the device and set autoincrement-on-read */
    w = STAT_NONE;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);  // Stop device (in fact, does nothing, since ADC4 just freezes until LAM reset)
    DO_NAF(CAMAC_REF, me->N_DEV, 0,      0,  &w);  // Dummy read -- reset LAM
    w = STAT_AINC;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);  // Set autoincrement mode

    cum_v = 0;
    dp = me->retdata;
    for (nl = 0;  nl < ADC4_NUM_LINES;  nl++)
    {
        /* Set read address */
        w = ADDR_EOM -
            me->cur_args[ADC4_CHAN_NUMPTS];
        DO_NAF(CAMAC_REF, me->N_DEV, A_ADDR, 16, &w);

        zero_shift = me->cur_args[ADC4_CHAN_ZERO0 + nl];

        if (me->cur_args[ADC4_CHAN_CALC_STATS] == 0)
        {
            for (n = me->cur_args[ADC4_CHAN_NUMPTS];  n > 0;  n -= count)
            {
                count = n;
                if (count > COUNT_MAX) count = COUNT_MAX;

                DO_NAFB(CAMAC_REF, N_DEV, nl, 0, rdata, count);
                for (x = 0;  x < count;  x++)
                {
                    v = rdata[x] & 0x0FFF;
                    cum_v |= (v - 1) | (v + 1);
                    adc4_code2uv(v, zero_shift, dp);
                    dp++;
                }
            }

            me->cur_args[ADC4_CHAN_MIN0 + nl] =
            me->nxt_args[ADC4_CHAN_MIN0 + nl] =
            me->cur_args[ADC4_CHAN_MAX0 + nl] =
            me->nxt_args[ADC4_CHAN_MAX0 + nl] =
            me->cur_args[ADC4_CHAN_AVG0 + nl] =
            me->nxt_args[ADC4_CHAN_AVG0 + nl] =
            me->cur_args[ADC4_CHAN_INT0 + nl] =
            me->nxt_args[ADC4_CHAN_INT0 + nl] = 0;
        }
        else
        {
            min_v = 0x0FFF; max_v = 0; sum_v = 0;
            for (n = me->cur_args[ADC4_CHAN_NUMPTS];  n > 0;  n -= count)
            {
                count = n;
                if (count > COUNT_MAX) count = COUNT_MAX;
    
                DO_NAFB(CAMAC_REF, N_DEV, nl, 0, rdata, count);
                for (x = 0;  x < count;  x++)
                {
                    v = rdata[x] & 0x0FFF;
                    cum_v |= (v - 1) | (v + 1);
                    adc4_code2uv(v, zero_shift, dp);
                    dp++;
                    if (min_v > v) min_v = v;
                    if (max_v < v) max_v = v;
                    sum_v += v;
                }
            }

            adc4_code2uv(min_v, zero_shift, &vd);
            me->cur_args[ADC4_CHAN_MIN0 + nl] =
            me->nxt_args[ADC4_CHAN_MIN0 + nl] = vd;

            adc4_code2uv(max_v, zero_shift, &vd);
            me->cur_args[ADC4_CHAN_MAX0 + nl] =
            me->nxt_args[ADC4_CHAN_MAX0 + nl] = vd;

            adc4_code2uv(sum_v / me->cur_args[ADC4_CHAN_NUMPTS], zero_shift, &vd);
            me->cur_args[ADC4_CHAN_AVG0 + nl] =
            me->nxt_args[ADC4_CHAN_AVG0 + nl] = vd;

            vd = sum_v - (2048 + zero_shift) * me->cur_args[ADC4_CHAN_NUMPTS];
            me->cur_args[ADC4_CHAN_INT0 + nl] =
            me->nxt_args[ADC4_CHAN_INT0 + nl] = vd;
        }
    }

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc4_privrec_t *me = PDR2ME(pdr);

  int             n;
  int             x;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[ADC4_CHAN_XS_PER_POINT] = 0;
    for (x = 0;  x < ADC4_NUM_LINES;  x++)
    {
        me->cur_args[ADC4_CHAN_LINE0ON       + x] = 1;
        me->cur_args[ADC4_CHAN_LINE0RANGEMIN + x] = ADC4_MIN_VALUE - me->cur_args[ADC4_CHAN_ZERO0 + x];
        me->cur_args[ADC4_CHAN_LINE0RANGEMAX + x] = ADC4_MAX_VALUE - me->cur_args[ADC4_CHAN_ZERO0 + x];
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC4_CHAN_CALC_STATS])
        for (x = ADC4_CHAN_STATS_first;  x <= ADC4_CHAN_STATS_last; x++)
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
        me->r.addrs [n] = ADC4_CHAN_DATA;
        me->r.dtypes[n] = ADC4_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC4_CHAN_NUMPTS] * ADC4_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0;  x < ADC4_NUM_LINES;  x++)
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC4_CHAN_LINE0 + x;
            me->r.dtypes[n] = ADC4_DTYPE;
            me->r.nelems[n] = me->cur_args[ADC4_CHAN_NUMPTS];
            me->r.val_ps[n] = me->retdata + me->cur_args[ADC4_CHAN_NUMPTS] * x;
            me->r.rflags[n] = add_rflags;
            n++;
        }
    /* A marker */
    me->r.addrs [n] = ADC4_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC4_CHAN_MARKER;
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
}

//////////////////////////////////////////////////////////////////////

enum
{
    PARAM_SHOT     = ADC4_CHAN_SHOT,
    PARAM_ISTART   = ADC4_CHAN_ISTART,
    PARAM_WAITTIME = ADC4_CHAN_WAITTIME,
    PARAM_STOP     = ADC4_CHAN_STOP,
    PARAM_ELAPSED  = ADC4_CHAN_ELAPSED,

    PARAM_DATA     = ADC4_CHAN_DATA,
    PARAM_LINE0    = ADC4_CHAN_LINE1,
};

#define FASTADC_NAME adc4
#include "camac_fastadc_common.h"

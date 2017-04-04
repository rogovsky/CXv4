#include "cxsd_driver.h"
#include "pzframe_drv.h"

#include "drv_i/adc502_drv_i.h"


enum
{
    A_DATA1    = 0,
    A_DATA2    = 1,
    A_ADRPTR   = 0,
};

enum
{
    S_RUN          = 1 << 0,
    S_REC          = 1 << 1,
//    S_Fn           = 1 << 2,
    S_TIMING_SHIFT =      2,
    S_TMODE_SHIFT  =      3,
    S_STM          = 1 << 4,
    S_BSY          = 1 << 5,
    S_OV           = 1 << 6,
    S_RSRVD8       = 1 << 7,
    S_SHIFT1_SHIFT =      8,
    S_RANGE1_SHIFT =     10,
    S_SHIFT2_SHIFT =     12,
    S_RANGE2_SHIFT =     14,
};

static int param2shiftcode[3] = {0, 1, 2};
static int param2base0    [3] = {2048, 1024, 3072};
static int param2quant    [4] = {39072, 19536, 9768, 4884};

typedef int32 ADC502_DATUM_T;
enum { ADC502_DTYPE = CXDTYPE_INT32 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC502_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC502_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC502_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC502_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC502_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC502_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC502_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC502_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC502_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC502_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_TMODE]         = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_RANGE1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_RANGE2]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_SHIFT1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC502_CHAN_SHIFT2]        = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC502_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC502_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC502_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_PTSOFS},
    [ADC502_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_NUMPTS},
    [ADC502_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_TIMING},
    [ADC502_CHAN_CUR_TMODE]     = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_TMODE},
    [ADC502_CHAN_CUR_RANGE1]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_RANGE1},
    [ADC502_CHAN_CUR_RANGE2]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_RANGE2},
    [ADC502_CHAN_CUR_SHIFT1]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_SHIFT1},
    [ADC502_CHAN_CUR_SHIFT2]    = {PZFRAME_CHTYPE_STATUS,      ADC502_CHAN_SHIFT2},

    [ADC502_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC502_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC502_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC502_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC502_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC502_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC502_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC502_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC502_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                N_DEV;

    int32              cur_args[ADC502_NUMCHANS];
    int32              nxt_args[ADC502_NUMCHANS];
    ADC502_DATUM_T     retdata[ADC502_MAX_NUMPTS*ADC502_NUM_LINES];
    pzframe_retbufs_t  retbufs;

    int                data_rqd;
    int                line_rqd[ADC502_NUM_LINES];
    struct
    {
        int            addrs [ADC502_NUMCHANS];
        cxdtype_t      dtypes[ADC502_NUMCHANS];
        int            nelems[ADC502_NUMCHANS];
        void          *val_ps[ADC502_NUMCHANS];
        rflags_t       rflags[ADC502_NUMCHANS];
    }                  r;

    int                cword;
} adc502_privrec_t;

#define PDR2ME(pdr) ((adc502_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static psp_paramdescr_t adc502_params[] =
{
    PSP_P_FLAG("calcstats", adc502_privrec_t, nxt_args[ADC502_CHAN_CALC_STATS], 1, 0),
    PSP_P_END()
};

//--------------------------------------------------------------------

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int v)
{
    if      (n == ADC502_CHAN_TIMING)
        v &= 1;
    else if (n == ADC502_CHAN_TMODE)
        v &= 1;
    else if (n == ADC502_CHAN_RANGE1  ||  n == ADC502_CHAN_RANGE2)
        v &= 3;
    else if (n == ADC502_CHAN_SHIFT1  ||  n == ADC502_CHAN_SHIFT2)
    {
        if (v != ADC502_SHIFT_NONE  &&
            v != ADC502_SHIFT_NEG4  &&
            v != ADC502_SHIFT_POS4)
        v = ADC502_SHIFT_NONE;
    }
    else if (n == ADC502_CHAN_PTSOFS)
    {
        if (v < 0)                   v = 0;
        if (v > ADC502_MAX_NUMPTS-1) v = ADC502_MAX_NUMPTS-1;
        
    }
    else if (n == ADC502_CHAN_NUMPTS)
    {
        if (v < 1)                   v = 1;
        if (v > ADC502_MAX_NUMPTS)   v = ADC502_MAX_NUMPTS;
    }

    return v;
}

static void Return1Param(adc502_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static int   InitParams(pzframe_drv_t *pdr)
{
  adc502_privrec_t *me = PDR2ME(pdr);

  int               n;

    for (n = 0;  n < countof(chinfo);  n++)
        if (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
            chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);

    me->nxt_args[ADC502_CHAN_NUMPTS] = 1024;
    me->nxt_args[ADC502_CHAN_TIMING] = ADC502_TIMING_INT;
    me->nxt_args[ADC502_CHAN_TMODE]  = ADC502_TMODE_HF;
    me->nxt_args[ADC502_CHAN_RANGE1] = ADC502_RANGE_8192;
    me->nxt_args[ADC502_CHAN_RANGE2] = ADC502_RANGE_8192;
    me->nxt_args[ADC502_CHAN_SHIFT1] = ADC502_SHIFT_NONE;
    me->nxt_args[ADC502_CHAN_SHIFT2] = ADC502_SHIFT_NONE;
    
    Return1Param(me, ADC502_CHAN_XS_DIVISOR, 0);
    Return1Param(me, ADC502_CHAN_XS_FACTOR,  -9);

    for (n = 0;  n < ADC502_NUM_LINES;  n++)
    {
        Return1Param(me, ADC502_CHAN_LINE1TOTALMIN + n, ADC502_MIN_VALUE);
        Return1Param(me, ADC502_CHAN_LINE1TOTALMAX + n, ADC502_MAX_VALUE);
    }
    Return1Param    (me, ADC502_CHAN_NUM_LINES,         ADC502_NUM_LINES);

    SetChanRDs(me->devid, ADC502_CHAN_DATA, 1+ADC502_NUM_LINES, 1000000.0, 0.0);
    SetChanRDs(me->devid, ADC502_CHAN_STATS_first,
                          ADC502_CHAN_STATS_last-ADC502_CHAN_STATS_first+1,
                                                                1000000.0, 0.0);
    SetChanRDs(me->devid, ADC502_CHAN_ELAPSED, 1, 1000.0, 0.0);

    return DEVSTATE_OPERATING;
}


static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc502_privrec_t *me = PDR2ME(pdr);

  int               cword;
  int               junk;
  int               w;

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Additional check of PTSOFS against NUMPTS */
    if (me->cur_args    [ADC502_CHAN_PTSOFS] > ADC502_MAX_NUMPTS - me->cur_args[ADC502_CHAN_NUMPTS])
        Return1Param(me, ADC502_CHAN_PTSOFS,   ADC502_MAX_NUMPTS - me->cur_args[ADC502_CHAN_NUMPTS]);

    /* Program the device */
    /* a. Drop LAM first... */
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 10, &junk);
    /* b. Prepare status/control word */
    cword = me->cword =
        /* common params */
        S_RUN * 0 | S_REC * 0 |
        (me->cur_args[ADC502_CHAN_TIMING] << S_TIMING_SHIFT) |
        (me->cur_args[ADC502_CHAN_TMODE]  << S_TMODE_SHIFT ) |
        S_STM * 1 |
        /* chan1 */
        (param2shiftcode[me->cur_args[ADC502_CHAN_SHIFT1]] << S_SHIFT1_SHIFT) |
        (                me->cur_args[ADC502_CHAN_RANGE1]  << S_RANGE1_SHIFT) |
        /* chan2 */
        (param2shiftcode[me->cur_args[ADC502_CHAN_SHIFT2]] << S_SHIFT2_SHIFT) |
        (                me->cur_args[ADC502_CHAN_RANGE2]  << S_RANGE2_SHIFT);

    ////DoDriverLog(devid, DRIVERLOG_DEBUG, "%s(), CWORD=0x%x", __FUNCTION__, cword);

    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // 1st write -- to clear RUN bit (?)
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // 2nd write -- to set all other bits
    /* c. Set "measurement size" */
    w = me->cur_args[ADC502_CHAN_NUMPTS] + me->cur_args[ADC502_CHAN_PTSOFS];
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 17, &w);
    /* d. Enable start */
    cword |= S_RUN;
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // Set RUN
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 26, &junk);  // Enable LAM
    
    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc502_privrec_t *me = PDR2ME(pdr);

  int               cword = me->cword;
  int               junk;

    cword &=~ S_RUN;
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // Drop RUN
    cword &=~ S_STM; me->cword = cword;
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // Drop STM to allow A(0)F(25)
    cword |=  S_RUN;
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // Restore RUN
    
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 25, &junk);  // Trigger!

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc502_privrec_t *me = PDR2ME(pdr);
  int               n;

  int               cword = me->cword &~ S_RUN;
  int               junk;

    DO_NAF(CAMAC_REF, me->N_DEV, 0, 24, &junk);  // Disable LAM
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // Drop RUN
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 10, &junk);  // Drop LAM
    
    if (me->cur_args[ADC502_CHAN_NUMPTS] != 0)
        bzero(me->retdata,
              sizeof(me->retdata[0])
              *
              me->cur_args[ADC502_CHAN_NUMPTS] * ADC502_NUM_LINES);

    for (n = ADC502_CHAN_STATS_first;  n <= ADC502_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc502_privrec_t *me = PDR2ME(pdr);

  int               cword = me->cword &~ S_RUN;
  int               junk;
  int               w;
  
  int               N_DEV = me->N_DEV;
  int               nl;
  ADC502_DATUM_T   *dp;
  int               n;
  int               base0;
  int               quant;
  enum             {COUNT_MAX = 100};
  int               count;
  int               x;
  int               rdata[COUNT_MAX];
  int               v;
  int32             min_v, max_v, sum_v;

    /* Stop the device */
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 24, &junk);  // Disable LAM
    DO_NAF(CAMAC_REF, me->N_DEV, 1, 17, &cword); // Drop RUN

    for (nl = 0;  nl < ADC502_NUM_LINES;  nl++)
    {
        w = me->cur_args[ADC502_CHAN_NUMPTS];
        DO_NAF(CAMAC_REF, N_DEV, 0, 17, &w);  // Set read address

        base0 = param2base0[me->cur_args[ADC502_CHAN_SHIFT1 + nl]];
        quant = param2quant[me->cur_args[ADC502_CHAN_RANGE1 + nl]];

        dp = me->retdata + nl * me->cur_args[ADC502_CHAN_NUMPTS];

        if (me->cur_args[ADC502_CHAN_CALC_STATS] == 0)
        {
            for (n = me->cur_args[ADC502_CHAN_NUMPTS];  n > 0;  n -= count)
            {
                count = n;
                if (count > COUNT_MAX) count = COUNT_MAX;
    
                DO_NAFB(CAMAC_REF, N_DEV, 0 + nl, 0, rdata, count);
                for (x = 0;  x < count;  x++)
                {
                    *dp++ = quant * ((rdata[x] & 0x0FFF) - base0) / 10;
                }
            }

            me->cur_args[ADC502_CHAN_MIN1 + nl] =
            me->nxt_args[ADC502_CHAN_MIN1 + nl] =
            me->cur_args[ADC502_CHAN_MAX1 + nl] =
            me->nxt_args[ADC502_CHAN_MAX1 + nl] =
            me->cur_args[ADC502_CHAN_AVG1 + nl] =
            me->nxt_args[ADC502_CHAN_AVG1 + nl] =
            me->cur_args[ADC502_CHAN_INT1 + nl] =
            me->nxt_args[ADC502_CHAN_INT1 + nl] = 0;
        }
        else
        {
            min_v = +99999; max_v = -99999; sum_v = 0;
            for (n = me->cur_args[ADC502_CHAN_NUMPTS];  n > 0;  n -= count)
            {
                count = n;
                if (count > COUNT_MAX) count = COUNT_MAX;
    
                DO_NAFB(CAMAC_REF, N_DEV, 0 + nl, 0, rdata, count);
                for (x = 0;  x < count;  x++)
                {
                    v = (rdata[x] & 0x0FFF) - base0;
                    *dp++ = quant * v / 10;
                    if (min_v > v) min_v = v;
                    if (max_v < v) max_v = v;
                    sum_v += v;
                }
            }

            me->cur_args[ADC502_CHAN_MIN1 + nl] =
            me->nxt_args[ADC502_CHAN_MIN1 + nl] = quant * min_v / 10;

            me->cur_args[ADC502_CHAN_MAX1 + nl] =
            me->nxt_args[ADC502_CHAN_MAX1 + nl] = quant * max_v / 10;

            me->cur_args[ADC502_CHAN_AVG1 + nl] =
            me->nxt_args[ADC502_CHAN_AVG1 + nl] = quant * (sum_v / me->cur_args[ADC502_CHAN_NUMPTS]) / 10;

            me->cur_args[ADC502_CHAN_INT1 + nl] =
            me->nxt_args[ADC502_CHAN_INT1 + nl] = sum_v;
        }
    }

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc502_privrec_t *me = PDR2ME(pdr);

  int               n;
  int               x;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[ADC502_CHAN_XS_PER_POINT] =
        (me->cur_args[ADC502_CHAN_TIMING] == ADC502_TIMING_INT)? 20 : 0;
    for (x = 0;  x < ADC502_NUM_LINES;  x++)
    {
        me->cur_args[ADC502_CHAN_LINE1ON       + x] = 1;
        me->cur_args[ADC502_CHAN_LINE1RANGEMIN + x] =
            param2quant         [me->cur_args[ADC502_CHAN_RANGE1 + x] & 3] *
            (    0 - param2base0[me->cur_args[ADC502_CHAN_SHIFT1 + x] & 3])
            / 10;
        me->cur_args[ADC502_CHAN_LINE1RANGEMAX + x] =
            param2quant         [me->cur_args[ADC502_CHAN_RANGE1 + x] & 3] *
            (0xFFF - param2base0[me->cur_args[ADC502_CHAN_SHIFT1 + x] & 3])
            / 10;
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC502_CHAN_CALC_STATS])
        for (x = ADC502_CHAN_STATS_first;  x <= ADC502_CHAN_STATS_last; x++)
        {
            me->r.addrs [n] = x;
            me->r.dtypes[n] = CXDTYPE_INT32;
            me->r.nelems[n] = 1;
            me->r.val_ps[n] = me->cur_args + x;
            me->r.rflags[n] = 0;
            n++;
        }

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
        me->r.addrs [n] = ADC502_CHAN_DATA;
        me->r.dtypes[n] = ADC502_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC502_CHAN_NUMPTS] * ADC502_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0;  x < ADC502_NUM_LINES;  x++)
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC502_CHAN_LINE1 + x;
            me->r.dtypes[n] = ADC502_DTYPE;
            me->r.nelems[n] = me->cur_args[ADC502_CHAN_NUMPTS];
            me->r.val_ps[n] = me->retdata + me->cur_args[ADC502_CHAN_NUMPTS] * x;
            me->r.rflags[n] = add_rflags;
            n++;
        }
    /* A marker */
    me->r.addrs [n] = ADC502_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC502_CHAN_MARKER;
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
    PARAM_SHOT     = ADC502_CHAN_SHOT,
    PARAM_ISTART   = ADC502_CHAN_ISTART,
    PARAM_WAITTIME = ADC502_CHAN_WAITTIME,
    PARAM_STOP     = ADC502_CHAN_STOP,
    PARAM_ELAPSED  = ADC502_CHAN_ELAPSED,

    PARAM_DATA     = ADC502_CHAN_DATA,
    PARAM_LINE0    = ADC502_CHAN_LINE1,
};

#define FASTADC_NAME adc502
#include "camac_fastadc_common.h"

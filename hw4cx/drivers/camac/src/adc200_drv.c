#include "cxsd_driver.h"
#include "pzframe_drv.h"

#include "drv_i/adc200_drv_i.h"


enum
{
    A_DATA1    = 0,
    A_DATA2    = 1,
    A_ADRPTR   = 0,
    A_BLKSTART = 1,
    A_BLKSIZE  = 2,
    A_STATUS   = 3,
    A_ZERO1    = 4,
    A_ZERO2    = 5,
};

enum
{
    S_RUN      = 1,
    S_DL       = 1 << 1,
    S_TMOD     = 1 << 2,
    S_LOOP     = 1 << 3,
    S_BLK      = 1 << 4,
    S_HLT      = 1 << 5,
    S_FDIV_SHIFT   = 6,
    S_FRQ200   = 1 << 8,
    S_FRQ195   = 1 << 9,
    S_FRQXTRN  = 1 << 10,
    S_SCALE1_SHIFT = 12,
    S_SCALE2_SHIFT = 14,
};

static int32  quants       [4] = {2000, 4000, 8000, 16000};
static int    timing_scales[4] = {5000/*1e9/200e6*/, 5128/*1e9/195e6*/, 0, 0};
static int    frqdiv_scales[4] = {1, 2, 4, 8};

typedef int32 ADC200_DATUM_T;
enum { ADC200_DTYPE = CXDTYPE_INT32 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC200_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC200_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC200_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC200_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC200_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC200_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_FRQDIV]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_RANGE1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_RANGE2]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_ZERO1]         = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200_CHAN_ZERO2]         = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC200_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_PTSOFS},
    [ADC200_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_NUMPTS},
    [ADC200_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_TIMING},
    [ADC200_CHAN_CUR_FRQDIV]    = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_FRQDIV},
    [ADC200_CHAN_CUR_RANGE1]    = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_RANGE1},
    [ADC200_CHAN_CUR_RANGE2]    = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_RANGE2},
    [ADC200_CHAN_CUR_ZERO1]     = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_ZERO1},
    [ADC200_CHAN_CUR_ZERO2]     = {PZFRAME_CHTYPE_STATUS,      ADC200_CHAN_ZERO2},

    [ADC200_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC200_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC200_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                N_DEV;

    int32              cur_args[ADC200_NUMCHANS];
    int32              nxt_args[ADC200_NUMCHANS];
    ADC200_DATUM_T     retdata [ADC200_MAX_NUMPTS*ADC200_NUM_LINES];
    pzframe_retbufs_t  retbufs;

    int                data_rqd;
    int                line_rqd[ADC200_NUM_LINES];
    struct
    {
        int            addrs [ADC200_NUMCHANS];
        cxdtype_t      dtypes[ADC200_NUMCHANS];
        int            nelems[ADC200_NUMCHANS];
        void          *val_ps[ADC200_NUMCHANS];
        rflags_t       rflags[ADC200_NUMCHANS];
    }                  r;

    int                cword;
} adc200_privrec_t;

#define PDR2ME(pdr) ((adc200_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static psp_lkp_t adc200_frqdiv_lkp[] =
{
    {"1",  ADC200_FRQD_5NS},
    {"5",  ADC200_FRQD_5NS},
    {"2",  ADC200_FRQD_10NS},
    {"10", ADC200_FRQD_10NS},
    {"4",  ADC200_FRQD_20NS},
    {"20", ADC200_FRQD_20NS},
    {"8",  ADC200_FRQD_40NS},
    {"40", ADC200_FRQD_40NS},
    {NULL, 0}
};

static psp_lkp_t adc200_timing_lkp[] =
{
    {"200",   ADC200_T_200MHZ},
    {"195",   ADC200_T_195MHZ},
    {"timer", ADC200_T_TIMER},
    {"ext",   ADC200_T_TIMER},
    {NULL, 0}
};

static psp_lkp_t adc200_range_lkp[] =
{
    {"256",  ADC200_R_256},
    {"512",  ADC200_R_512},
    {"1024", ADC200_R_1024},
    {"2048", ADC200_R_2048},
    {NULL, 0}
};

static psp_paramdescr_t adc200_params[] =
{
    PSP_P_INT   ("ptsofs",   adc200_privrec_t, nxt_args[ADC200_CHAN_PTSOFS], 0,    0, ADC200_MAX_NUMPTS-1),
    PSP_P_INT   ("numpts",   adc200_privrec_t, nxt_args[ADC200_CHAN_NUMPTS], 1024, 1, ADC200_MAX_NUMPTS),
    PSP_P_LOOKUP("frqdiv",   adc200_privrec_t, nxt_args[ADC200_CHAN_FRQDIV], ADC200_FRQD_5NS, adc200_frqdiv_lkp),
    PSP_P_LOOKUP("timing",   adc200_privrec_t, nxt_args[ADC200_CHAN_TIMING], ADC200_T_200MHZ, adc200_timing_lkp),
    PSP_P_LOOKUP("range1",   adc200_privrec_t, nxt_args[ADC200_CHAN_RANGE1], ADC200_R_2048,   adc200_range_lkp),
    PSP_P_LOOKUP("range2",   adc200_privrec_t, nxt_args[ADC200_CHAN_RANGE2], ADC200_R_2048,   adc200_range_lkp),
    PSP_P_INT   ("zero1",    adc200_privrec_t, nxt_args[ADC200_CHAN_ZERO1],  128,  0, 255),
    PSP_P_INT   ("zero2",    adc200_privrec_t, nxt_args[ADC200_CHAN_ZERO2],  128,  0, 255),

    PSP_P_FLAG("istart",     adc200_privrec_t, nxt_args[ADC200_CHAN_ISTART],     1, 0),
    PSP_P_FLAG("noistart",   adc200_privrec_t, nxt_args[ADC200_CHAN_ISTART],     0, 0),
    PSP_P_FLAG("calcstats",  adc200_privrec_t, nxt_args[ADC200_CHAN_CALC_STATS], 1, 0),
    PSP_P_FLAG("nocalcstats",adc200_privrec_t, nxt_args[ADC200_CHAN_CALC_STATS], 0, 0),
    PSP_P_END()
};

//--------------------------------------------------------------------

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int32 v)
{
    if      (n == ADC200_CHAN_TIMING)
    {
        if (v < ADC200_T_200MHZ) v = ADC200_T_200MHZ;
        if (v > ADC200_T_TIMER)  v = ADC200_T_TIMER;
    }
    else if (n == ADC200_CHAN_FRQDIV)
        v &= 3;
    else if (n == ADC200_CHAN_RANGE1  ||  n == ADC200_CHAN_RANGE2)
        v &= 3;
    else if (n == ADC200_CHAN_ZERO1   ||  n == ADC200_CHAN_ZERO2)
    {
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
    }
    else if (n == ADC200_CHAN_PTSOFS)
    {
        v &=~ 1;
        if (v < 0)                   v = 0;
        if (v > ADC200_MAX_NUMPTS-2) v = ADC200_MAX_NUMPTS-2;
    }
    else if (n == ADC200_CHAN_NUMPTS)
    {
        v = (v + 1) &~ 1;
        if (v < 2)                   v = 2;
        if (v > ADC200_MAX_NUMPTS)   v = ADC200_MAX_NUMPTS;
    }

    return v;
}

static void Return1Param(adc200_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}
 
static int   InitParams(pzframe_drv_t *pdr)
{
  adc200_privrec_t *me = PDR2ME(pdr);

  int               n;

//    me->nxt_args[ADC200_CHAN_NUMPTS] = 1024;
//    me->nxt_args[ADC200_CHAN_TIMING] = ADC200_T_200MHZ;
//    me->nxt_args[ADC200_CHAN_FRQDIV] = ADC200_FRQD_5NS;
//    me->nxt_args[ADC200_CHAN_RANGE1] = ADC200_R_2048;
//    me->nxt_args[ADC200_CHAN_RANGE2] = ADC200_R_2048;
//    me->nxt_args[ADC200_CHAN_ZERO1]  = 128;
//    me->nxt_args[ADC200_CHAN_ZERO2]  = 128;

    Return1Param(me, ADC200_CHAN_XS_FACTOR,  -9);

    for (n = 0;  n < ADC200_NUM_LINES;  n++)
    {
        Return1Param(me, ADC200_CHAN_LINE1TOTALMIN + n, ADC200_MIN_VALUE);
        Return1Param(me, ADC200_CHAN_LINE1TOTALMAX + n, ADC200_MAX_VALUE);
    }
    Return1Param    (me, ADC200_CHAN_NUM_LINES,         ADC200_NUM_LINES);

    SetChanRDs(me->devid, ADC200_CHAN_DATA, 1+ADC200_NUM_LINES, 1000000.0, 0.0);
    SetChanRDs(me->devid, ADC200_CHAN_STATS_first,
                          ADC200_CHAN_STATS_last-ADC200_CHAN_STATS_first+1,
                                                                1000000.0, 0.0);
    SetChanRDs(me->devid, ADC200_CHAN_ELAPSED, 1, 1000.0, 0.0);

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc200_privrec_t *me = PDR2ME(pdr);

  int               statword;
  int               w;
  int               junk;
  
  static int timing2frq[] = {S_FRQ200, S_FRQ195, S_FRQXTRN};
    
////fprintf(stderr, "%s %p %d\n", __FUNCTION__, me, me->N_DEV);
    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Additional check of PTSOFS against NUMPTS */
    if (me->cur_args    [ADC200_CHAN_PTSOFS] > ADC200_MAX_NUMPTS - me->cur_args[ADC200_CHAN_NUMPTS])
        Return1Param(me, ADC200_CHAN_PTSOFS,   ADC200_MAX_NUMPTS - me->cur_args[ADC200_CHAN_NUMPTS]);

    /* Program the device */
    /* a. Prepare... */
    DO_NAF(CAMAC_REF, me->N_DEV, 0,          10, &junk);  // Drop LAM *first*
    statword = S_RUN * 0 | S_DL * 0 | S_TMOD * 1 | S_LOOP * 0 | S_BLK * 0 |
               S_HLT * 0 |
               (me->cur_args[ADC200_CHAN_FRQDIV] << S_FDIV_SHIFT) |
               timing2frq[me->cur_args[ADC200_CHAN_TIMING]] |
               (me->cur_args[ADC200_CHAN_RANGE1] << S_SCALE1_SHIFT) |
               (me->cur_args[ADC200_CHAN_RANGE2] << S_SCALE2_SHIFT);
    DO_NAF(CAMAC_REF, me->N_DEV, A_STATUS,   17, &statword);
    /* b. Set parameters */
    w = 0;
    DO_NAF(CAMAC_REF, me->N_DEV, A_BLKSTART, 17, &w); // "Block start counter" := 0
    w = (me->cur_args[ADC200_CHAN_NUMPTS] + 1 + me->cur_args[ADC200_CHAN_PTSOFS]) / 2;
    //w = 2; /////
    DO_NAF(CAMAC_REF, me->N_DEV, A_BLKSIZE,  17, &w); // "Block size" := number + offset
    w = me->cur_args[ADC200_CHAN_ZERO1];
    DO_NAF(CAMAC_REF, me->N_DEV, A_ZERO1,    17, &w); // Set chan1 zero
    w = me->cur_args[ADC200_CHAN_ZERO2];
    DO_NAF(CAMAC_REF, me->N_DEV, A_ZERO2,    17, &w); // Set chan2 zero
    /* c. Drop LAM */
    DO_NAF(CAMAC_REF, me->N_DEV, 0,          10, &junk);
    /* d. Start */
    statword &=~ S_HLT;
    statword |=  S_RUN;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STATUS,   17, &statword);

    me->cword = statword;

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc200_privrec_t *me = PDR2ME(pdr);

  int               junk;
    
////fprintf(stderr, "%s %p %d\n", __FUNCTION__, me, me->N_DEV);
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 25, &junk);

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc200_privrec_t *me = PDR2ME(pdr);
  int               n;

  int               w;
  int               junk;
  
////fprintf(stderr, "%s %p %d\n", __FUNCTION__, me, me->N_DEV);
    w = (me->cword &~ S_RUN) | S_HLT;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STATUS, 17, &w);
    DO_NAF(CAMAC_REF, me->N_DEV, 0,        10, &junk);

    if (me->cur_args[ADC200_CHAN_NUMPTS] != 0)
        bzero(me->retdata,
              sizeof(me->retdata[0])
              *
              me->cur_args[ADC200_CHAN_NUMPTS] * ADC200_NUM_LINES);

    for (n = ADC200_CHAN_STATS_first;  n <= ADC200_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}


static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc200_privrec_t *me = PDR2ME(pdr);

  int               numduplets;
  int               N_DEV = me->N_DEV;
  int               w;
  int               junk;
  
  int               nl;
  ADC200_DATUM_T   *dp;
  int               n;
  enum             {COUNT_MAX = 100};
  int               count;
  int               x;
  int               rdata[COUNT_MAX];
  int32             shift;
  int32             scale;
  int32             v;
  int32             min_v, max_v, sum_v;

////fprintf(stderr, "%s %p %d\n", __FUNCTION__, me, me->N_DEV);
    /* Stop the device */
    w = (me->cword &~ S_RUN) | S_DL | S_HLT*0;
    DoDriverLog(me->devid, DRIVERLOG_DEBUG, "START cword=%x, READ cword=%x", me->cword, w);
    DO_NAF(CAMAC_REF, me->N_DEV, A_STATUS, 17, &w);
    DO_NAF(CAMAC_REF, me->N_DEV, 0,        10, &junk);

    numduplets = (me->cur_args[ADC200_CHAN_NUMPTS] + 1) / 2;
    for (nl = 0;  nl < ADC200_NUM_LINES;  nl++)
    {
        shift =       (me->cur_args[ADC200_CHAN_ZERO1  + nl] & 255) - 256;
        scale = quants[me->cur_args[ADC200_CHAN_RANGE1 + nl] & 3];
        dp = me->retdata + nl * me->cur_args[ADC200_CHAN_NUMPTS];
        w = me->cur_args[ADC200_CHAN_PTSOFS] / 2;
        DO_NAF(CAMAC_REF, N_DEV, A_ADRPTR, 17, &w);  // Set read address

        if (me->cur_args[ADC200_CHAN_CALC_STATS] == 0)
        {
            for (n = numduplets;  n > 0;  n -= count)
            {
                count = n;
                if (count > COUNT_MAX) count = COUNT_MAX;
    
                DO_NAFB(CAMAC_REF, N_DEV, A_DATA1 + nl, 0, rdata, count);
                for (x = 0;  x < count;  x++)
                {
                    w = rdata[x];
                    *dp++ =  ((w       & 0xFF) + shift) * scale;
                    *dp++ = (((w >> 8) & 0xFF) + shift) * scale;
                }
            }

            me->cur_args[ADC200_CHAN_MIN1 + nl] =
            me->nxt_args[ADC200_CHAN_MIN1 + nl] =
            me->cur_args[ADC200_CHAN_MAX1 + nl] =
            me->nxt_args[ADC200_CHAN_MAX1 + nl] =
            me->cur_args[ADC200_CHAN_AVG1 + nl] =
            me->nxt_args[ADC200_CHAN_AVG1 + nl] =
            me->cur_args[ADC200_CHAN_INT1 + nl] =
            me->nxt_args[ADC200_CHAN_INT1 + nl] = 0;
        }
        else
        {
            min_v = ADC200_MAX_VALUE; max_v = ADC200_MIN_VALUE; sum_v = 0;
            for (n = numduplets;  n > 0;  n -= count)
            {
                count = n;
                if (count > COUNT_MAX) count = COUNT_MAX;
    
                DO_NAFB(CAMAC_REF, N_DEV, A_DATA1 + nl, 0, rdata, count);
                for (x = 0;  x < count;  x++)
                {
                    v =  ((rdata[x]       & 0xFF) + shift) * scale;
                    *dp++ = v;
                    if (min_v > v) min_v = v;
                    if (max_v < v) max_v = v;
                    sum_v += v;

                    v = (((rdata[x] >> 8) & 0xFF) + shift) * scale;
                    *dp++ = v;
                    if (min_v > v) min_v = v;
                    if (max_v < v) max_v = v;
                    sum_v += v;
                }
            }

            me->cur_args[ADC200_CHAN_MIN1 + nl] =
            me->nxt_args[ADC200_CHAN_MIN1 + nl] = min_v;

            me->cur_args[ADC200_CHAN_MAX1 + nl] =
            me->nxt_args[ADC200_CHAN_MAX1 + nl] = max_v;

            me->cur_args[ADC200_CHAN_AVG1 + nl] =
            me->nxt_args[ADC200_CHAN_AVG1 + nl] = sum_v / me->cur_args[ADC200_CHAN_NUMPTS];

            me->cur_args[ADC200_CHAN_INT1 + nl] =
            me->nxt_args[ADC200_CHAN_INT1 + nl] = sum_v;
        }
    }

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc200_privrec_t *me = PDR2ME(pdr);

  int               n;
  int               x;

////fprintf(stderr, "%s %p %d\n", __FUNCTION__, me, me->N_DEV);
    n = 0;

    /* 1. Device-specific: calc  */
    me->cur_args[ADC200_CHAN_XS_PER_POINT] =
        timing_scales[me->cur_args[ADC200_CHAN_TIMING] & 3];
    me->cur_args[ADC200_CHAN_XS_DIVISOR] =
        1000 / frqdiv_scales[me->cur_args[ADC200_CHAN_FRQDIV] & 3];
    for (x = 0;  x < ADC200_NUM_LINES;  x++)
    {
        me->cur_args[ADC200_CHAN_LINE1ON       + x] = 1;
        me->cur_args[ADC200_CHAN_LINE1RANGEMIN + x] =
            ((  0 + (me->cur_args[ADC200_CHAN_ZERO1  + x] & 255)) - 256) *
              quants[me->cur_args[ADC200_CHAN_RANGE1 + x] & 3];
        me->cur_args[ADC200_CHAN_LINE1RANGEMAX + x] =
            ((255 + (me->cur_args[ADC200_CHAN_ZERO1  + x] & 255)) - 256) *
              quants[me->cur_args[ADC200_CHAN_RANGE1 + x] & 3];
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC200_CHAN_CALC_STATS])
        for (x = ADC200_CHAN_STATS_first;  x <= ADC200_CHAN_STATS_last; x++)
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
        me->r.addrs [n] = ADC200_CHAN_DATA;
        me->r.dtypes[n] = ADC200_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC200_CHAN_NUMPTS] * ADC200_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0;  x < ADC200_NUM_LINES;  x++)
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC200_CHAN_LINE1 + x;
            me->r.dtypes[n] = ADC200_DTYPE;
            me->r.nelems[n] = me->cur_args[ADC200_CHAN_NUMPTS];
            me->r.val_ps[n] = me->retdata + me->cur_args[ADC200_CHAN_NUMPTS] * x;
            me->r.rflags[n] = add_rflags;
            n++;
        }
    /* A marker */
    me->r.addrs [n] = ADC200_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC200_CHAN_MARKER;
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
    PARAM_SHOT     = ADC200_CHAN_SHOT,
    PARAM_ISTART   = ADC200_CHAN_ISTART,
    PARAM_WAITTIME = ADC200_CHAN_WAITTIME,
    PARAM_STOP     = ADC200_CHAN_STOP,
    PARAM_ELAPSED  = ADC200_CHAN_ELAPSED,

    PARAM_DATA     = ADC200_CHAN_DATA,
    PARAM_LINE0    = ADC200_CHAN_LINE1,
};

#define FASTADC_NAME adc200
#include "camac_fastadc_common.h"

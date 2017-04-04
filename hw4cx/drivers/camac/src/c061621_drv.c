#include "cxsd_driver.h"
#include "pzframe_drv.h"

#include "drv_i/c061621_drv_i.h"


enum
{
    DEVTYPE_ADC101S  = 001,
    DEVTYPE_ADC102S  = 002,
    DEVTYPE_STROBE_S = 003,
    DEVTYPE_ADC850S  = 004,
    DEVTYPE_ADC710S  = 005,
    DEVTYPE_ADC101SK = 011,
    DEVTYPE_ADC850SK = 014,
};
static const char *devnames[] =
{
    [DEVTYPE_ADC101S]  = "ADC-101S",
    [DEVTYPE_ADC102S]  = "ADC-102S",
    [DEVTYPE_STROBE_S] = "STROBE-S",
    [DEVTYPE_ADC850S]  = "ADC-850S",
    [DEVTYPE_ADC710S]  = "ADC-710S",
    [DEVTYPE_ADC101SK] = "ADC-101SK",
    [DEVTYPE_ADC850SK] = "ADC-850SK",
};

enum
{
    A_MEMORY = 0,
    A_STATUS = 1,
    A_MEMADR = 2,
    A_LIMITS = 3,
    A_AUXDAT = 4,
    A_PRGRUN = 5,
    A_DOTICK = 6,
    A_COMREG = 7,  // K-suffixed devices
    A_INDREG = 15, // 710S/710SK only?
};

enum
{
    S_CPU_RAM = 1 << 0,
    S_DIS_LAM = 1 << 1,
    S_SNG_RUN = 1 << 2,
    S_UNUSED3 = 1 << 3,
    S_AUTOPLT = 1 << 4,
};

static int tick_codes[] =
{
    // Digit 1: 0:x1, 1:x2, 2:x4, 3:x5
    // Digit 2:
    //   0:0.1ns
    //   1:  1ns
    //   2: 10ns
    //   2:100ns
    //   4:  1us
    //   5: 10us
    //   6:100us
    //   7:  1ms

    032, // 50  ns
    003, // 100 ns
    013, // 200 ns
    023, // 400 ns
    033, // 500 ns
    004, // 1   us
    014, // 2   us
    024, // 4   us
    034, // 5   us
    005, // 10  us
    015, // 20  us
    025, // 40  us
    035, // 50  us
    006, // 100 us
    016, // 200 us
    026, // 400 us
    036, // 500 us
    007, // 1   ms
    017, // 2   ms
    027, // CPU
    037, // Ext
};

static int param2quant[8] = { 390,   780, 1560,   3120,
                             6250, 12500, 25000, 50000};
static int q2xs[] =
{
    50, 100, 200, 400, 500,
    1000, 2000, 4000, 5000, 10000, 20000, 40000, 50000,
    100000, 200000, 400000, 500000, 1000000, 2000000,
    0, 0
};


typedef int32 C061621_DATUM_T;
enum { C061621_DTYPE = CXDTYPE_INT32 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [C061621_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},

    /* controls */
    [C061621_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [C061621_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [C061621_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [C061621_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [C061621_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [C061621_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [C061621_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [C061621_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [C061621_CHAN_RANGE]         = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [C061621_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [C061621_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [C061621_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      C061621_CHAN_PTSOFS},
    [C061621_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      C061621_CHAN_NUMPTS},
    [C061621_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      C061621_CHAN_TIMING},
    [C061621_CHAN_CUR_RANGE]     = {PZFRAME_CHTYPE_STATUS,      C061621_CHAN_RANGE},

    [C061621_CHAN_ON]            = {PZFRAME_CHTYPE_STATUS,      -1},
    [C061621_CHAN_RANGEMIN]      = {PZFRAME_CHTYPE_STATUS,      -1},
    [C061621_CHAN_RANGEMAX]      = {PZFRAME_CHTYPE_STATUS,      -1},

    [C061621_CHAN_MIN]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_MAX]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_AVG]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_INT]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [C061621_CHAN_TOTALMIN]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_TOTALMAX]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [C061621_CHAN_DEVTYPE_ID]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [C061621_CHAN_DEVTYPE_STR]   = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                N_DEV;

    int                memsize;
    int                mintick;
    int                k_type;

    int32              cur_args[C061621_NUMCHANS];
    int32              nxt_args[C061621_NUMCHANS];
    C061621_DATUM_T    retdata [C061621_MAX_NUMPTS*C061621_NUM_LINES];
    pzframe_retbufs_t  retbufs;

    int                data_rqd;
    int                line_rqd[0];
    struct
    {
        int            addrs [C061621_NUMCHANS];
        cxdtype_t      dtypes[C061621_NUMCHANS];
        int            nelems[C061621_NUMCHANS];
        void          *val_ps[C061621_NUMCHANS];
        rflags_t       rflags[C061621_NUMCHANS];
    }                  r;
} c061621_privrec_t;

#define PDR2ME(pdr) ((c061621_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static psp_paramdescr_t c061621_params[] =
{
    PSP_P_FLAG("calcstats", c061621_privrec_t, nxt_args[C061621_CHAN_CALC_STATS], 1, 0),
    PSP_P_END()
};

//--------------------------------------------------------------------

static void SetToPassive(c061621_privrec_t *me)
{
  int         w;
  int         junk;

    w = S_CPU_RAM | S_DIS_LAM;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STATUS, 16, &w);
    DO_NAF(CAMAC_REF, me->N_DEV,        0, 10, &junk); // Drom LAM
}

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int v)
{
  c061621_privrec_t *me = PDR2ME(pdr);

    if      (n == C061621_CHAN_TIMING)
    {
        if (v < me->mintick)             v = me->mintick;
        if (v > countof(tick_codes) - 1) v = countof(tick_codes) - 1;
    }
    else if (n == C061621_CHAN_RANGE)
    {
        if (v < 0) v = 0;
        if (v > 7) v = 7;
        if (me->k_type  &&  v < 4) v = 4; // K-type devices have limited range
    }
    else if (n == C061621_CHAN_PTSOFS)
    {
        if (v < 0)               v = 0;
        if (v > me->memsize - 1) v = me->memsize - 1;
    }
    else if (n == C061621_CHAN_NUMPTS)
    {
        if (v < 1)               v = 1;
        if (v > me->memsize)     v = me->memsize;
    }

    return v;
}

static void Return1Param(c061621_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void Init1Param(c061621_privrec_t *me, int n, int32 v)
{
    if (me->nxt_args[n] < 0) me->nxt_args[n] = v;
}
static int   InitParams(pzframe_drv_t *pdr)
{
  c061621_privrec_t *me = PDR2ME(pdr);

  rflags_t           rflags;
  int                w;
  int                devtype;
  const char        *devname;

  int                n;

    rflags = status2rflags(DO_NAF(CAMAC_REF, me->N_DEV, A_AUXDAT, 1, &w));
    if (rflags != 0)
    {
        DoDriverLog(me->devid, 0,
                    "reading devtype, NAF(%d,%d,%d): X=%d,Q=%d, aborting",
                    me->N_DEV, A_AUXDAT, 1,
                    rflags & CXRF_CAMAC_NO_X? 0 : 1,
                    rflags & CXRF_CAMAC_NO_Q? 0 : 1);
        return -rflags;
    }

    devtype = w & 017;
    devname = NULL;
    if (devtype < countof(devnames)) devname = devnames[devtype];
    if (devname == NULL) devname = "???";

    SetToPassive(me);

    switch (devtype)
    {
        case DEVTYPE_ADC101S:
            me->memsize = 4096;
            me->mintick = 5;
            DoDriverLog(me->devid, 0, "%s, memsize=%d",
                        devname, me->memsize);
            break;

        case DEVTYPE_ADC101SK:
            me->memsize = 4096;
            me->mintick = 5;
            DoDriverLog(me->devid, 0, "%s, using as %s with memsize=%d",
                        devname, devnames[devtype & 07], me->memsize);
            break;

        case DEVTYPE_ADC850S:
            me->memsize = 1024;
            me->mintick = 0;
            DoDriverLog(me->devid, 0, "%s, memsize=%d",
                        devname, me->memsize);
            break;

        case DEVTYPE_ADC850SK:
            me->memsize = 4096;
            me->mintick = 0;
            DoDriverLog(me->devid, 0, "%s, using as %s with memsize=%d",
                        devname, devnames[devtype & 07], me->memsize);
            break;

        default:
            DoDriverLog(me->devid, 0, "unsupported devtype=%o:%s", devtype, devname);
            return -CXRF_WRONG_DEV;
    }
    me->k_type = (devtype & 010) != 0;

    for (n = 0;  n < countof(chinfo);  n++)
        if (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
            chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);

    me->nxt_args[C061621_CHAN_NUMPTS] = me->memsize;
    me->nxt_args[C061621_CHAN_TIMING] = me->mintick;
    me->nxt_args[C061621_CHAN_RANGE]  = ValidateParam(&(me->pz), C061621_CHAN_RANGE, 0);

    Return1Param(me, C061621_CHAN_XS_DIVISOR, 0);
    Return1Param(me, C061621_CHAN_XS_FACTOR,  -9);

    Return1Param(me, C061621_CHAN_TOTALMIN,  C061621_MIN_VALUE);
    Return1Param(me, C061621_CHAN_TOTALMAX,  C061621_MAX_VALUE);
    Return1Param(me, C061621_CHAN_NUM_LINES, C061621_NUM_LINES);

    SetChanRDs(me->devid, C061621_CHAN_DATA,    1, 1000000.0, 0.0);
    SetChanRDs(me->devid, C061621_CHAN_ELAPSED, 1, 1000.0,    0.0);

    /* Take care of DEVTYPE_ID... */
    Return1Param(me, C061621_CHAN_DEVTYPE_ID,  devtype);
    /* ...and DEVTYPE_STR */
    SetChanReturnType(me->devid, C061621_CHAN_DEVTYPE_STR, 1, IS_AUTOUPDATED_TRUSTED);
    ReturnDataSet(me->devid, 1,
                  (int      []){C061621_CHAN_DEVTYPE_STR},
                  (cxdtype_t[]){CXDTYPE_TEXT},
                  (int      []){strlen(devname)},
                  (void *   []){&devname},
                  (rflags_t []){0},
                  NULL);

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  c061621_privrec_t *me = PDR2ME(pdr);

  int  w;
  int  range_l;
  int  range_k;

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Additional check of PTSOFS against NUMPTS */
    if (me->cur_args    [C061621_CHAN_PTSOFS] > me->memsize - me->cur_args[C061621_CHAN_NUMPTS])
        Return1Param(me, C061621_CHAN_PTSOFS,   me->memsize - me->cur_args[C061621_CHAN_NUMPTS]);

    if (me->k_type)
    {
        range_l = 4;
        range_k = me->cur_args[C061621_CHAN_RANGE] - 4;
    }
    else
    {
        range_l = me->cur_args[C061621_CHAN_RANGE];
        range_k = 0;
    }

    /* Program the device */
    SetToPassive(me);
    w = (range_l << 6) | tick_codes[me->cur_args[C061621_CHAN_TIMING]];
    DO_NAF(CAMAC_REF, me->N_DEV, A_LIMITS, 16, &w);
    w = (range_k * 0xAA) + (0 << 8); // Force chan=0
    if (me->k_type) DO_NAF(CAMAC_REF, me->N_DEV, A_COMREG, 16, &w);
    w = S_SNG_RUN;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STATUS, 16, &w);

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  c061621_privrec_t *me = PDR2ME(pdr);

  int  junk;
    
    DO_NAF(CAMAC_REF, me->N_DEV, A_PRGRUN, 16, &junk);

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  c061621_privrec_t *me = PDR2ME(pdr);

    SetToPassive(me);

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  c061621_privrec_t *me = PDR2ME(pdr);

  int              w;
  int              base0;
  int              quant;
  C061621_DATUM_T *dp;
  int              n;
  enum            {COUNT_MAX = 100};
  int              count;
  int              x;
  int              rdata[COUNT_MAX];
  int              v;
  int32            min_v, max_v, sum_v;

    w = me->cur_args[C061621_CHAN_PTSOFS];
    DO_NAF(CAMAC_REF, me->N_DEV, A_MEMADR, 16, &w); // Set read address

    quant = param2quant[me->cur_args[C061621_CHAN_RANGE]];
    base0 = 4095 /* ==2047.5*2 */ * quant / 2;

    dp = me->retdata;

    if (me->cur_args[C061621_CHAN_CALC_STATS] == 0  ||  1)
    {
        for (n = me->cur_args[C061621_CHAN_NUMPTS];  n > 0;  n -= count)
        {
            count = n;
            if (count > COUNT_MAX) count = COUNT_MAX;
            
            DO_NAFB(CAMAC_REF, me->N_DEV, A_MEMORY, 0, rdata, count);
            for (x = 0;  x < count;  x++)
            {
                *dp++ = quant * (rdata[x] & 0x0FFF) - base0;
            }
        }

        me->cur_args[C061621_CHAN_MIN] =
        me->nxt_args[C061621_CHAN_MIN] =
        me->cur_args[C061621_CHAN_MAX] =
        me->nxt_args[C061621_CHAN_MAX] =
        me->cur_args[C061621_CHAN_AVG] =
        me->nxt_args[C061621_CHAN_AVG] =
        me->cur_args[C061621_CHAN_INT] =
        me->nxt_args[C061621_CHAN_INT] = 0;
    }

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  c061621_privrec_t *me = PDR2ME(pdr);

  int                n;
  int                x;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[C061621_CHAN_XS_PER_POINT] =
        q2xs[me->cur_args[C061621_CHAN_TIMING]];
    me->cur_args[C061621_CHAN_ON] = 1;
    me->cur_args[C061621_CHAN_RANGEMIN] = C061621_MIN_VALUE; /*!!!*/
    me->cur_args[C061621_CHAN_RANGEMAX] = C061621_MAX_VALUE; /*!!!*/

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[C061621_CHAN_CALC_STATS])
        for (x = C061621_CHAN_STATS_first;  x <= C061621_CHAN_STATS_last; x++)
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
        me->r.addrs [n] = C061621_CHAN_DATA;
        me->r.dtypes[n] = C061621_DTYPE;
        me->r.nelems[n] = me->cur_args[C061621_CHAN_NUMPTS] * C061621_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    /* ...and drop "requested" flags */
    me->data_rqd = 0;

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
    PARAM_SHOT     = C061621_CHAN_SHOT,
    PARAM_ISTART   = C061621_CHAN_ISTART,
    PARAM_WAITTIME = C061621_CHAN_WAITTIME,
    PARAM_STOP     = C061621_CHAN_STOP,
    PARAM_ELAPSED  = C061621_CHAN_ELAPSED,

    PARAM_DATA     = C061621_CHAN_DATA,
    PARAM_LINE0    = -1,
};

#define FASTADC_NAME c061621
#include "camac_fastadc_common.h"

#include "cxsd_driver.h"
#include "pzframe_drv.h"

#include "drv_i/f4226_drv_i.h"


static int q2xs[] = {50, 100, 200, 400, 800, 1600, 3200, 0};


typedef int32 F4226_DATUM_T;
enum { F4226_DTYPE = CXDTYPE_INT32 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [F4226_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},

    [F4226_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [F4226_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [F4226_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [F4226_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [F4226_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [F4226_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [F4226_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [F4226_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [F4226_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [F4226_CHAN_RANGE]         = {PZFRAME_CHTYPE_VALIDATE,    0},
    [F4226_CHAN_PREHIST64]     = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [F4226_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [F4226_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [F4226_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      F4226_CHAN_PTSOFS},
    [F4226_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      F4226_CHAN_NUMPTS},
    [F4226_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      F4226_CHAN_TIMING},
    [F4226_CHAN_CUR_RANGE]     = {PZFRAME_CHTYPE_STATUS,      F4226_CHAN_RANGE},
    [F4226_CHAN_CUR_PREHIST64] = {PZFRAME_CHTYPE_STATUS,      F4226_CHAN_PREHIST64},

    [F4226_CHAN_ON]            = {PZFRAME_CHTYPE_STATUS,      -1},
    [F4226_CHAN_RANGEMIN]      = {PZFRAME_CHTYPE_STATUS,      -1},
    [F4226_CHAN_RANGEMAX]      = {PZFRAME_CHTYPE_STATUS,      -1},

    [F4226_CHAN_MIN]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_MAX]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_AVG]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_INT]           = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [F4226_CHAN_TOTALMIN]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_TOTALMAX]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [F4226_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                N_DEV;

    int32              cur_args[F4226_NUMCHANS];
    int32              nxt_args[F4226_NUMCHANS];
    F4226_DATUM_T      retdata [F4226_MAX_NUMPTS*F4226_NUM_LINES];
    pzframe_retbufs_t  retbufs;

    int                data_rqd;
    int                line_rqd[0];
    struct
    {
        int            addrs [F4226_NUMCHANS];
        cxdtype_t      dtypes[F4226_NUMCHANS];
        int            nelems[F4226_NUMCHANS];
        void          *val_ps[F4226_NUMCHANS];
        rflags_t       rflags[F4226_NUMCHANS];
    }                  r;
} f4226_privrec_t;

#define PDR2ME(pdr) ((f4226_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static psp_lkp_t f4226_prehist64_lkp[] =
{
    {"0",    0},
    {"64",   1},
    {"128",  2},
    {"192",  3},
    {"256",  4},
    {"320",  5},
    {"384",  6},
    {"448",  7},
    {"512",  8},
    {"576",  9},
    {"640", 10},
    {"704", 11},
    {"768", 12},
    {"832", 13},
    {"896", 14},
    {"960", 15},
    {NULL, 0},
};

static psp_paramdescr_t f4226_params[] =
{
    PSP_P_LOOKUP("prehist", f4226_privrec_t, nxt_args[F4226_CHAN_PREHIST64], -1, f4226_prehist64_lkp),
    PSP_P_FLAG("calcstats", f4226_privrec_t, nxt_args[F4226_CHAN_CALC_STATS], 1, 0),
    PSP_P_END()
};

//--------------------------------------------------------------------

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int v)
{
  f4226_privrec_t *me = PDR2ME(pdr);

    if      (n == F4226_CHAN_TIMING)
    {
        if (v < 0)               v = 0;
        if (v > F4226_T_EXT - 1) v = F4226_T_EXT - 1;
    }
    else if (n == F4226_CHAN_RANGE)
    {
        if (v < 0)               v = 0;
        if (v > F4226_TST_256MV) v = F4226_TST_256MV;
    }
    else if (n == F4226_CHAN_PREHIST64)
    {
        if (v < 0)  v = 0;
        if (v > 15) v = 15;
    }
    else if (n == F4226_CHAN_PTSOFS)
    {
        if (v < 0)                    v = 0;
        if (v > F4226_MAX_NUMPTS - 1) v = F4226_MAX_NUMPTS - 1;
    }
    else if (n == F4226_CHAN_NUMPTS)
    {
        if (v < 1)                    v = 1;
        if (v > F4226_MAX_NUMPTS)     v = F4226_MAX_NUMPTS;
    }

    return v;
}

static void Return1Param(f4226_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void Init1Param(f4226_privrec_t *me, int n, int32 v)
{
    if (me->nxt_args[n] < 0) me->nxt_args[n] = v;
}
static int   InitParams(pzframe_drv_t *pdr)
{
  f4226_privrec_t *me = PDR2ME(pdr);

  rflags_t         rflags;
  int              w;

    rflags = status2rflags(DO_NAF(CAMAC_REF, me->N_DEV, 0, 1, &w));

    if (me->nxt_args[F4226_CHAN_PREHIST64] < 0)
        me->nxt_args[F4226_CHAN_PREHIST64] = (w >> 7) & 0xF;

    /*!!! Should read status reg!!! */
    me->nxt_args[F4226_CHAN_NUMPTS] = F4226_MAX_NUMPTS;
    me->nxt_args[F4226_CHAN_TIMING] = F4226_INP_256MV;
    me->nxt_args[F4226_CHAN_RANGE]  = ValidateParam(&(me->pz), F4226_CHAN_RANGE, 0);

    Return1Param(me, F4226_CHAN_XS_DIVISOR, 0);
    Return1Param(me, F4226_CHAN_XS_FACTOR,  -9);

    Return1Param(me, F4226_CHAN_TOTALMIN,  F4226_MIN_VALUE);
    Return1Param(me, F4226_CHAN_TOTALMAX,  F4226_MAX_VALUE);
    Return1Param(me, F4226_CHAN_NUM_LINES, F4226_NUM_LINES);

    SetChanRDs(me->devid, F4226_CHAN_DATA,    1, 1000000.0, 0.0);
    SetChanRDs(me->devid, F4226_CHAN_ELAPSED, 1, 1000.0,    0.0);

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  f4226_privrec_t *me = PDR2ME(pdr);

  int              w;

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Additional check of PTSOFS against NUMPTS */
    if (me->cur_args    [F4226_CHAN_PTSOFS] > F4226_MAX_NUMPTS - me->cur_args[F4226_CHAN_NUMPTS])
        Return1Param(me, F4226_CHAN_PTSOFS,   F4226_MAX_NUMPTS - me->cur_args[F4226_CHAN_NUMPTS]);

    /* Program the device */
    /*!!!*/

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  f4226_privrec_t *me = PDR2ME(pdr);

  int              junk;
    
    /*!!!*/

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  f4226_privrec_t *me = PDR2ME(pdr);

    /*!!!*/

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  f4226_privrec_t *me = PDR2ME(pdr);

  int              w;
  int              base0;
  int              quant;
  F4226_DATUM_T   *dp;
  int              n;
  enum            {COUNT_MAX = 100};
  int              count;
  int              x;
  int              rdata[COUNT_MAX];
  int              v;
  int32            min_v, max_v, sum_v;

    /*!!!*/

    dp = me->retdata;

    if (me->cur_args[F4226_CHAN_CALC_STATS] == 0  ||  1)
    {
        for (n = me->cur_args[F4226_CHAN_NUMPTS];  n > 0;  n -= count)
        {
            count = n;
            if (count > COUNT_MAX) count = COUNT_MAX;
            
    /*!!!*/
//            DO_NAFB(CAMAC_REF, me->N_DEV, A_MEMORY, 0, rdata, count);
//            for (x = 0;  x < count;  x++)
//            {
//                *dp++ = quant * (rdata[x] & 0x0FFF) - base0;
//            }
        }

        me->cur_args[F4226_CHAN_MIN] =
        me->nxt_args[F4226_CHAN_MIN] =
        me->cur_args[F4226_CHAN_MAX] =
        me->nxt_args[F4226_CHAN_MAX] =
        me->cur_args[F4226_CHAN_AVG] =
        me->nxt_args[F4226_CHAN_AVG] =
        me->cur_args[F4226_CHAN_INT] =
        me->nxt_args[F4226_CHAN_INT] = 0;
    }

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  f4226_privrec_t *me = PDR2ME(pdr);

  int              n;
  int              x;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[F4226_CHAN_XS_PER_POINT] =
        q2xs[me->cur_args[F4226_CHAN_TIMING]];
    me->cur_args[F4226_CHAN_ON] = 1;
    me->cur_args[F4226_CHAN_RANGEMIN] = F4226_MIN_VALUE; /*!!!*/
    me->cur_args[F4226_CHAN_RANGEMAX] = F4226_MAX_VALUE; /*!!!*/

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[F4226_CHAN_CALC_STATS])
        for (x = F4226_CHAN_STATS_first;  x <= F4226_CHAN_STATS_last; x++)
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
        me->r.addrs [n] = F4226_CHAN_DATA;
        me->r.dtypes[n] = F4226_DTYPE;
        me->r.nelems[n] = me->cur_args[F4226_CHAN_NUMPTS] * F4226_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    /* A marker (just for compatibility with others) */
    me->r.addrs [n] = F4226_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + F4226_CHAN_MARKER;
    me->r.rflags[n] = add_rflags;
    n++;
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

    /* F4226-specific: adjust CUR_PTSOFS value by PREHIST64*64 */
    me->nxt_args[F4226_CHAN_CUR_PTSOFS] = me->cur_args[F4226_CHAN_CUR_PTSOFS] =
        me->cur_args[F4226_CHAN_PTSOFS] - me->cur_args[F4226_CHAN_PREHIST64]*64;
}

//////////////////////////////////////////////////////////////////////

enum
{
    PARAM_SHOT     = F4226_CHAN_SHOT,
    PARAM_ISTART   = F4226_CHAN_ISTART,
    PARAM_WAITTIME = F4226_CHAN_WAITTIME,
    PARAM_STOP     = F4226_CHAN_STOP,
    PARAM_ELAPSED  = F4226_CHAN_ELAPSED,

    PARAM_DATA     = F4226_CHAN_DATA,
    PARAM_LINE0    = -1,
};

#define FASTADC_NAME f4226
#include "camac_fastadc_common.h"

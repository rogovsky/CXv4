#include "cxsd_driver.h"
#include "pzframe_drv.h"

#include "drv_i/adc333_drv_i.h"


enum
{
    A_ANY  = 0,
    
    A_DATA = 0,
    A_ADDR = 8,
    A_STAT = 9,
    A_LIMS = 10
};

enum {DIVISOR = 1008}; // ==0 on %2, %3, %4
static int timing_scales[8] = {DIVISOR/2, 1*DIVISOR, 2*DIVISOR, 4*DIVISOR, 8*DIVISOR, 16*DIVISOR, 32*DIVISOR, 0/*ext*/}; // 0.5, 1, 2, 4, 16, 32, EXT ms/datum
static int32      quants[4] = {4000, 2000, 1000, 500}; // ={8192,4096,2048,1024}*1000/2048

typedef int32 ADC333_DATUM_T;
enum { ADC333_DTYPE = CXDTYPE_INT32 };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC333_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC333_CHAN_LINE0]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC333_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC333_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC333_CHAN_LINE3]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC333_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC333_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC333_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC333_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC333_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC333_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC333_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC333_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC333_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC333_CHAN_RANGE0]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC333_CHAN_RANGE1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC333_CHAN_RANGE2]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC333_CHAN_RANGE3]        = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC333_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC333_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_PTSOFS},
    [ADC333_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_NUMPTS},
    [ADC333_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_TIMING},
    [ADC333_CHAN_CUR_RANGE0]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_RANGE0},
    [ADC333_CHAN_CUR_RANGE1]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_RANGE1},
    [ADC333_CHAN_CUR_RANGE2]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_RANGE2},
    [ADC333_CHAN_CUR_RANGE3]    = {PZFRAME_CHTYPE_STATUS,      ADC333_CHAN_RANGE3},

    [ADC333_CHAN_LINE0ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE3ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE0RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE3RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE0RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC333_CHAN_LINE3RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC333_CHAN_MIN0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MIN3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MAX0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_MAX3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_AVG0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_AVG3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_INT0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_INT3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC333_CHAN_LINE0TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE3TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE0TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_LINE3TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC333_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                N_DEV;

    int32              cur_args[ADC333_NUMCHANS];
    int32              nxt_args[ADC333_NUMCHANS];
    ADC333_DATUM_T     retdata[ADC333_MAX_NUMPTS];
    pzframe_retbufs_t  retbufs;

    int                data_rqd;
    int                line_rqd[ADC333_NUM_LINES];
    struct
    {
        int            addrs [ADC333_NUMCHANS];
        cxdtype_t      dtypes[ADC333_NUMCHANS];
        int            nelems[ADC333_NUMCHANS];
        void          *val_ps[ADC333_NUMCHANS];
        rflags_t       rflags[ADC333_NUMCHANS];
    }                  r;

    int              nlines;
    int              curscales[4];
} adc333_privrec_t;

#define PDR2ME(pdr) ((adc333_privrec_t*)pdr) //!!! Should better sufbtract offsetof(pz)

static psp_lkp_t adc333_timing_lkp[] =
{
    {"500", ADC333_T_500NS},
    {"0.5", ADC333_T_500NS},
    {"1",   ADC333_T_1US},
    {"2",   ADC333_T_2US},
    {"4",   ADC333_T_4US},
    {"8",   ADC333_T_8US},
    {"16",  ADC333_T_16US},
    {"32",  ADC333_T_32US},
    {"ext", ADC333_T_EXT},
    {NULL, 0}
};

static psp_lkp_t adc333_range_lkp[] =
{
    {"8192", ADC333_R_8192},
    {"4096", ADC333_R_4096},
    {"2048", ADC333_R_2048},
    {"1024", ADC333_R_1024},
    {"off",  ADC333_R_OFF},
    {NULL, 0}
};

static psp_paramdescr_t adc333_params[] =
{
    PSP_P_INT    ("ptsofs",    adc333_privrec_t, nxt_args[ADC333_CHAN_PTSOFS],    0,    0, ADC333_MAX_NUMPTS-1),
    PSP_P_INT    ("numpts",    adc333_privrec_t, nxt_args[ADC333_CHAN_NUMPTS],    1024, 1, ADC333_MAX_NUMPTS),
    PSP_P_LOOKUP ("timing",    adc333_privrec_t, nxt_args[ADC333_CHAN_TIMING],    ADC333_T_500NS, adc333_timing_lkp),
    PSP_P_LOOKUP ("range0",    adc333_privrec_t, nxt_args[ADC333_CHAN_RANGE0],    ADC333_R_8192, adc333_range_lkp),
    PSP_P_LOOKUP ("range1",    adc333_privrec_t, nxt_args[ADC333_CHAN_RANGE1],    ADC333_R_8192, adc333_range_lkp),
    PSP_P_LOOKUP ("range2",    adc333_privrec_t, nxt_args[ADC333_CHAN_RANGE2],    ADC333_R_8192, adc333_range_lkp),
    PSP_P_LOOKUP ("range3",    adc333_privrec_t, nxt_args[ADC333_CHAN_RANGE3],    ADC333_R_8192, adc333_range_lkp),

    PSP_P_FLAG("istart",       adc333_privrec_t, nxt_args[ADC333_CHAN_ISTART],     1, 0),
    PSP_P_FLAG("noistart",     adc333_privrec_t, nxt_args[ADC333_CHAN_ISTART],     0, 0),
    PSP_P_FLAG("calcstats",    adc333_privrec_t, nxt_args[ADC333_CHAN_CALC_STATS], 1, 0),
    PSP_P_FLAG("nocalcstats",  adc333_privrec_t, nxt_args[ADC333_CHAN_CALC_STATS], 0, 0),

    PSP_P_END()
};

//--------------------------------------------------------------------

static inline rflags_t code2uv(uint16 code, int scale, ADC333_DATUM_T *r_p)
{
    /* Overflow? */
    if ((code & (1 << 12)) != 0)
    {
        *r_p = (code & (1 << 11)) != 0? +100000000 : -100000000;
        return CXRF_OVERLOAD;
    }

    *r_p = ((int)(code & 0x0FFF) - 2048) * scale;
    
    return 0;
}


static int32 ValidateParam(pzframe_drv_t *pdr, int n, int v)
{
    if      (n == ADC333_CHAN_TIMING)
        v &= 7;
    else if (n >= ADC333_CHAN_RANGE0  &&  n <= ADC333_CHAN_RANGE3)
        v = (v >= ADC333_R_8192  &&  v <= ADC333_R_1024)? v : ADC333_R_OFF;
    else if (n == ADC333_CHAN_PTSOFS)
    {
        if (v < 0)                   v = 0;
        if (v > ADC333_MAX_NUMPTS-1) v = ADC333_MAX_NUMPTS-1;
    }
    else if (n == ADC333_CHAN_NUMPTS)
    {
        if (v < 1)                   v = 1;
        if (v > ADC333_MAX_NUMPTS)   v = ADC333_MAX_NUMPTS;
    }

    return v;
}

static void Return1Param(adc333_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void Init1Param(adc333_privrec_t *me, int n, int32 v)
{
    if (me->nxt_args[n] < 0) me->nxt_args[n] = v;
}
static int   InitParams(pzframe_drv_t *pdr)
{
  adc333_privrec_t *me = PDR2ME(pdr);

  int               n;

//    Init1Param(me, ADC333_CHAN_PTSOFS, 0);
//    Init1Param(me, ADC333_CHAN_NUMPTS, 1024);
//    Init1Param(me, ADC333_CHAN_TIMING, ADC333_T_500NS);
//    Init1Param(me, ADC333_CHAN_RANGE0, ADC333_R_8192);
//    Init1Param(me, ADC333_CHAN_RANGE1, ADC333_R_8192);
//    Init1Param(me, ADC333_CHAN_RANGE2, ADC333_R_8192);
//    Init1Param(me, ADC333_CHAN_RANGE3, ADC333_R_8192);

    Return1Param(me, ADC333_CHAN_XS_FACTOR,  -9);

    for (n = 0;  n < ADC333_NUM_LINES;  n++)
    {
        Return1Param(me, ADC333_CHAN_LINE0TOTALMIN + n, ADC333_MIN_VALUE);
        Return1Param(me, ADC333_CHAN_LINE0TOTALMAX + n, ADC333_MAX_VALUE);
    }
    Return1Param    (me, ADC333_CHAN_NUM_LINES,         ADC333_NUM_LINES);

    SetChanRDs(me->devid, ADC333_CHAN_DATA, 1+ADC333_NUM_LINES, 1000000.0, 0.0);
    SetChanRDs(me->devid, ADC333_CHAN_STATS_first,
                          ADC333_CHAN_STATS_last-ADC333_CHAN_STATS_first+1,
                                                                1000000.0, 0.0);
    SetChanRDs(me->devid, ADC333_CHAN_ELAPSED, 1, 1000.0, 0.0);

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc333_privrec_t *me = PDR2ME(pdr);

  int               nl;
  int               nlines;
  int               rcode;

  int               regv_status;
  int               regv_limits;

  int               w;
  int               junk;

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Calculate # of lines and prepare scale factors */
    for (nl = 0, nlines = 0, regv_status = 0, regv_limits = (me->cur_args[ADC333_CHAN_TIMING] << 8);
         nl < ADC333_NUM_LINES;
         nl++)
    {
        rcode = me->cur_args[ADC333_CHAN_RANGE0 + nl];
        if (rcode >= ADC333_R_8192  &&  rcode <= ADC333_R_1024)
        {
            regv_status |= (1 << (nl + 4));
            regv_limits |= (rcode << (nl*2));
            me->curscales[nlines] = quants[rcode];
            nlines++;
        }
    }
    me->nlines = nlines;
    DoDriverLog(me->devid, DRIVERLOG_DEBUG, "scales={%d,%d,%d,%d} regv_status=%08x regv_limits=%08x", me->curscales[0], me->curscales[1], me->curscales[2], me->curscales[3], regv_status, regv_limits);

    /* No lines?! */
    if (nlines == 0)
    {
        return PZFRAME_R_READY;
    }

    /* Check NUMPTS */
    if (me->cur_args    [ADC333_CHAN_NUMPTS] > ADC333_MAX_NUMPTS / nlines)
        Return1Param(me, ADC333_CHAN_NUMPTS,   ADC333_MAX_NUMPTS / nlines);

    /* Check PTSOFS */
    if (me->cur_args    [ADC333_CHAN_PTSOFS] > ADC333_MAX_NUMPTS / nlines - me->cur_args[ADC333_CHAN_NUMPTS])
        Return1Param(me, ADC333_CHAN_PTSOFS,   ADC333_MAX_NUMPTS / nlines - me->cur_args[ADC333_CHAN_NUMPTS]);

    /* Program the device */
    /* a. Set parameters */
    regv_status |= 1;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &regv_status);  // Set status with HLT=1
    DO_NAF(CAMAC_REF, me->N_DEV, A_LIMS, 16, &regv_limits);  // Set limits
    w = (me->cur_args[ADC333_CHAN_NUMPTS] +
         me->cur_args[ADC333_CHAN_PTSOFS])
        * nlines;
    DO_NAF(CAMAC_REF, me->N_DEV, A_ADDR, 16, &w);            // Set address counter
    DO_NAF(CAMAC_REF, me->N_DEV, 0,      10, &junk);         // Drop LAM
    DO_NAF(CAMAC_REF, me->N_DEV, A_ANY,  26, &junk);         // Enable LAM
    /* b. Start */
    regv_status &=~ 1;
    DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &regv_status);  // Set status with HLT=0

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc333_privrec_t *me = PDR2ME(pdr);

  int               junk;
    
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 25, &junk);

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc333_privrec_t *me = PDR2ME(pdr);
  int               n;

  int               w;
  size_t            datasize;

    w = 0; DO_NAF(CAMAC_REF, me->N_DEV, A_ANY,  24, &w);     // Disable LAM
    w = 1; DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);     // Stop (HLT:=1)
    w = 0; DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 10, &w);     // Reset LAM

    datasize = sizeof(me->retdata[0])
               *
               me->cur_args[ADC333_CHAN_NUMPTS] * me->nlines;
    if (datasize != 0) bzero(me->retdata, datasize);

    for (n = ADC333_CHAN_STATS_first;  n <= ADC333_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc333_privrec_t *me = PDR2ME(pdr);

  int               numpts;
  int               N_DEV = me->N_DEV;
  int               w;

  rflags_t          rfv;
  int               qq;
  ADC333_DATUM_T   *p0, *p1, *p2, *p3;
  int               s0,  s1,  s2,  s3;

  enum             {COUNT_MAX = 108}; // ==0 on %2, %3, %4
  int               count;
  int               x;
  int               rdata[COUNT_MAX];
  
    w = 0; DO_NAF(CAMAC_REF, me->N_DEV, A_ANY,  24, &w);     // Disable LAM
    w = 1; DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 16, &w);     // Stop (HLT:=1)
    w = 0; DO_NAF(CAMAC_REF, me->N_DEV, A_STAT, 10, &w);     // Reset LAM

    numpts = me->cur_args[ADC333_CHAN_NUMPTS];
    w = numpts * me->nlines;
    DO_NAF(CAMAC_REF, me->N_DEV, A_ADDR, 16, &w);            // Set address counter
    rfv = 0;
    
    s0 = me->curscales[0];
    s1 = me->curscales[1];
    s2 = me->curscales[2];
    s3 = me->curscales[3];

    switch (me->nlines)
    {
        case 1:
            p0 = me->retdata + numpts * 0;
            for (qq = 0; numpts > 0;  numpts -= count /* / 1 */)
            {
                count = numpts /* * 1 */;
                if (count > COUNT_MAX) count = COUNT_MAX;

                qq |= ~(DO_NAFB(CAMAC_REF, N_DEV, A_DATA, 0, rdata, count));
                for (x = 0;  x < count;  p0++)
                {
                    rfv |= code2uv(rdata[x++], s0, p0);
                }
            }
            break;

        case 2:
            p0 = me->retdata + numpts * 0;
            p1 = me->retdata + numpts * 1;
            for (qq = 0; numpts > 0;  numpts -= count / 2)
            {
                count = numpts * 2;
                if (count > COUNT_MAX) count = COUNT_MAX;

                qq |= ~(DO_NAFB(CAMAC_REF, N_DEV, A_DATA, 0, rdata, count));
                for (x = 0;  x < count;  p0++, p1++)
                {
                    rfv |= code2uv(rdata[x++], s0, p0);
                    rfv |= code2uv(rdata[x++], s1, p1);
                }
            }
            break;

        case 3:
            p0 = me->retdata + numpts * 0;
            p1 = me->retdata + numpts * 1;
            p2 = me->retdata + numpts * 2;
            for (qq = 0; numpts > 0;  numpts -= count / 3)
            {
                count = numpts * 3;
                if (count > COUNT_MAX) count = COUNT_MAX;

                qq |= ~(DO_NAFB(CAMAC_REF, N_DEV, A_DATA, 0, rdata, count));
                for (x = 0;  x < count;  p0++, p1++, p2++)
                {
                    rfv |= code2uv(rdata[x++], s0, p0);
                    rfv |= code2uv(rdata[x++], s1, p1);
                    rfv |= code2uv(rdata[x++], s2, p2);
                }
            }
            break;

        case 4:
            p0 = me->retdata + numpts * 0; 
            p1 = me->retdata + numpts * 1; 
            p2 = me->retdata + numpts * 2; 
            p3 = me->retdata + numpts * 3; 
            for (qq = 0; numpts > 0;  numpts -= count / 4)
            {
                count = numpts * 4;
                if (count > COUNT_MAX) count = COUNT_MAX;

                qq |= ~(DO_NAFB(CAMAC_REF, N_DEV, A_DATA, 0, rdata, count));
                for (x = 0;  x < count;  p0++, p1++, p2++, p3++)
                {
                    rfv |= code2uv(rdata[x++], s0, p0);
                    rfv |= code2uv(rdata[x++], s1, p1);
                    rfv |= code2uv(rdata[x++], s2, p2);
                    rfv |= code2uv(rdata[x++], s3, p3);
                }
            }
            break;

    }

    /*!!! Note: CALC_STATS isn't implementedAACC */

    return rfv;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc333_privrec_t *me = PDR2ME(pdr);

  int               n;
  int               x;
  ADC333_DATUM_T   *bufp;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[ADC333_CHAN_XS_PER_POINT] =
        timing_scales[me->cur_args[ADC333_CHAN_TIMING] & 7];
    me->cur_args[ADC333_CHAN_XS_DIVISOR]   =
        DIVISOR / me->nlines;
    for (x = 0;  x < ADC333_NUM_LINES;  x++)
    {
        me->cur_args[ADC333_CHAN_LINE0ON       + x] = me->cur_args[ADC333_CHAN_RANGE0 + x] != ADC333_R_OFF;
        me->cur_args[ADC333_CHAN_LINE0RANGEMIN + x] = (    0-2048) * quants[me->cur_args[ADC333_CHAN_RANGE0 + x] & 3];
        me->cur_args[ADC333_CHAN_LINE0RANGEMAX + x] = (0xFFF-2048) * quants[me->cur_args[ADC333_CHAN_RANGE0 + x] & 3];
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC333_CHAN_CALC_STATS])
        for (x = ADC333_CHAN_STATS_first;  x <= ADC333_CHAN_STATS_last; x++)
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
        me->r.addrs [n] = ADC333_CHAN_DATA;
        me->r.dtypes[n] = ADC333_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC333_CHAN_NUMPTS] * me->nlines;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0, bufp = me->retdata;  x < ADC333_NUM_LINES;  x++)
    {
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC333_CHAN_LINE0 + x;
            me->r.dtypes[n] = ADC333_DTYPE;
            me->r.nelems[n] =
                (me->cur_args[ADC333_CHAN_LINE0ON + x])? me->cur_args[ADC333_CHAN_NUMPTS]
                                                       : 0;
            me->r.val_ps[n] = bufp;
            me->r.rflags[n] = add_rflags;
            n++;
        }
        if (me->cur_args[ADC333_CHAN_LINE0ON + x])
            bufp += me->cur_args[ADC333_CHAN_NUMPTS];
    }
    /* A marker */
    me->r.addrs [n] = ADC333_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC333_CHAN_MARKER;
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
    PARAM_SHOT     = ADC333_CHAN_SHOT,
    PARAM_ISTART   = ADC333_CHAN_ISTART,
    PARAM_WAITTIME = ADC333_CHAN_WAITTIME,
    PARAM_STOP     = ADC333_CHAN_STOP,
    PARAM_ELAPSED  = ADC333_CHAN_ELAPSED,

    PARAM_DATA     = ADC333_CHAN_DATA,
    PARAM_LINE0    = ADC333_CHAN_LINE0,
};

#define FASTADC_NAME adc333
#include "camac_fastadc_common.h"

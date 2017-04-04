#include "fastadc_data.h"
#include "adc4_data.h"
#include "drv_i/adc4_drv_i.h"


enum { ADC4_DTYPE      = CXDTYPE_INT32 };

static void adc4_info2mes(fastadc_data_t      *adc,
                          pzframe_chan_data_t *info,
                          fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < ADC4_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[ADC4_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[ADC4_CHAN_LINE0RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[ADC4_CHAN_LINE0RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[ADC4_CHAN_LINE0 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = ADC4_DTYPE;
    }

    mes_p->exttim = 1;
}

static int    adc4_x2xs(pzframe_chan_data_t *info __attribute__((unused)), int x)
{
    return x;
}

#define DSCR_L1(n,pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    [__CX_CONCATENATE(ADC4_CHAN_,__CX_CONCATENATE(pfx,__CX_CONCATENATE(n,sfx)))] = {np __CX_STRINGIZE(n) ns, _change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn}
#define DSCR_X4(   pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems) \
    DSCR_L1(0, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(1, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(2, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(3, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1)
#define DSCR_X4RDS(pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    DSCR_L1(0, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+0), \
    DSCR_L1(1, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+1), \
    DSCR_L1(2, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+2), \
    DSCR_L1(3, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+3)

static pzframe_chan_dscr_t adc4_chan_dscrs[] =
{
    DSCR_X4   (LINE,,           "line","",       0, PZFRAME_CHAN_IS_FRAME, 
                                                    ADC4_DTYPE,
                                                    ADC4_MAX_NUMPTS),
    [ADC4_CHAN_MARKER]       = {"marker",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC4_CHAN_SHOT]         = {"shot",          0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC4_CHAN_STOP]         = {"stop",          0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC4_CHAN_ISTART]       = {"istart"},
    [ADC4_CHAN_WAITTIME]     = {"waittime"},
    [ADC4_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC4_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC4_CHAN_NUMPTS]       = {"numpts"},
    DSCR_X4   (ZERO,,           "zero","",       0, 0, 0, 0),

    [ADC4_CHAN_ELAPSED]      = {"elapsed",       0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4_CHAN_XS_PER_POINT] = {"xs_per_point",  1},
    [ADC4_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC4_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC4_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",    1},
    [ADC4_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    DSCR_X4   (CUR_ZERO,,       "cur_zero","",   1, 0, 0, 0),

    DSCR_X4   (LINE,ON,       "line","on",       0, 0, 0, 0),
    DSCR_X4RDS(LINE,RANGEMIN, "line","rangemin", 0, 0, 0, 0, ADC4_CHAN_LINE0),
    DSCR_X4RDS(LINE,RANGEMAX, "line","rangemax", 0, 0, 0, 0, ADC4_CHAN_LINE0),

    DSCR_X4RDS(MIN,,            "min","",        0, 0, 0, 0, ADC4_CHAN_LINE0),
    DSCR_X4RDS(MAX,,            "max","",        0, 0, 0, 0, ADC4_CHAN_LINE0),
    DSCR_X4RDS(AVG,,            "avg","",        0, 0, 0, 0, ADC4_CHAN_LINE0),
    DSCR_X4RDS(INT,,            "int","",        0, 0, 0, 0, ADC4_CHAN_LINE0),

    // LINE[0-3]TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC4_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(ADC4_CHAN_LINE,x),                 \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1.0,                                                \
     {.int_r={ADC4_MIN_VALUE, ADC4_MAX_VALUE}},          \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC4_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC4_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc4_line_dscrs[] =
{
    LINE_DSCR("A", 0),
    LINE_DSCR("B", 1),
    LINE_DSCR("C", 2),
    LINE_DSCR("D", 3),
};

pzframe_type_dscr_t *adc4_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc4",
                               0,
                               adc4_chan_dscrs, countof(adc4_chan_dscrs),
                               /* ADC characteristics */
                               ADC4_MAX_NUMPTS, ADC4_NUM_LINES,
                               ADC4_CHAN_CUR_NUMPTS,
                               ADC4_CHAN_CUR_PTSOFS,
                               ADC4_CHAN_XS_PER_POINT,
                               ADC4_CHAN_XS_DIVISOR,
                               ADC4_CHAN_XS_FACTOR,
                               adc4_info2mes,
                               adc4_line_dscrs,
                               /* Data specifics */
                               "Channel",
                               "% 4.0f",
                               adc4_x2xs, "xs");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

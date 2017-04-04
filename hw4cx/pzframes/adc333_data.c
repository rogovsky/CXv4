#include "fastadc_data.h"
#include "adc333_data.h"
#include "drv_i/adc333_drv_i.h"


enum { ADC333_DTYPE      = CXDTYPE_INT32 };

static void adc333_info2mes(fastadc_data_t      *adc,
                            pzframe_chan_data_t *info,
                            fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < ADC333_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = info[ADC333_CHAN_CUR_RANGE0    + nl].valbuf.i32 != ADC333_R_OFF;
        mes_p->plots[nl].numpts             = info[ADC333_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[ADC333_CHAN_LINE0RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[ADC333_CHAN_LINE0RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        if (mes_p->plots[nl].on)
        {
            mes_p->plots[nl].x_buf          = info[ADC333_CHAN_LINE0 + nl].current_val;
            mes_p->plots[nl].x_buf_dtype    = ADC333_DTYPE;
        }
        else
        {
            mes_p->plots[nl].x_buf          = NULL;
            mes_p->plots[nl].numpts         = 0;
        }
    }

    mes_p->exttim = (info[ADC333_CHAN_TIMING].valbuf.i32 == ADC333_T_EXT);
}

static int    adc333_x2xs(pzframe_chan_data_t *info, int x)
{
  int        nl;
  int        num_on;

  static int timing_scales[8] = {500, 1000, 2000, 4000, 8000, 16000, 32000, 1000/*ext*/};

    for (nl = 0, num_on = 0;  nl < ADC333_NUM_LINES;  nl++)
        num_on += (info[ADC333_CHAN_CUR_RANGE0 + nl].valbuf.i32 != ADC333_R_OFF);

    return
        (x + info[ADC333_CHAN_CUR_PTSOFS].valbuf.i32)
        *
        num_on
        *
        timing_scales[info[ADC333_CHAN_CUR_TIMING].valbuf.i32 & 7]
        /
        1000;
}

#define DSCR_L1(n,pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    [__CX_CONCATENATE(ADC333_CHAN_,__CX_CONCATENATE(pfx,__CX_CONCATENATE(n,sfx)))] = {np __CX_STRINGIZE(n) ns, _change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn}
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

static pzframe_chan_dscr_t adc333_chan_dscrs[] =
{
    DSCR_X4   (LINE,,             "line","",      0, PZFRAME_CHAN_IS_FRAME, 
                                                     ADC333_DTYPE,
                                                     ADC333_MAX_NUMPTS),
    [ADC333_CHAN_MARKER]       = {"marker",       0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC333_CHAN_SHOT]         = {"shot",         0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC333_CHAN_STOP]         = {"stop",         0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC333_CHAN_ISTART]       = {"istart"},
    [ADC333_CHAN_WAITTIME]     = {"waittime"},
    [ADC333_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC333_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC333_CHAN_NUMPTS]       = {"numpts"},
    [ADC333_CHAN_TIMING]       = {"timing"},
    DSCR_X4   (RANGE,,            "range","",     0, 0, 0, 0),

    [ADC333_CHAN_ELAPSED]      = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC333_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [ADC333_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC333_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC333_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [ADC333_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [ADC333_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    DSCR_X4   (CUR_RANGE,,        "cur_range","", 1, 0, 0, 0),

    DSCR_X4   (LINE,ON,        "line","on",       0, 0, 0, 0),
    DSCR_X4RDS(LINE,RANGEMIN,  "line","rangemin", 0, 0, 0, 0, ADC333_CHAN_LINE0),
    DSCR_X4RDS(LINE,RANGEMAX,  "line","rangemax", 0, 0, 0, 0, ADC333_CHAN_LINE0),

    DSCR_X4RDS(MIN,,              "min","",       0, 0, 0, 0, ADC333_CHAN_LINE0),
    DSCR_X4RDS(MAX,,              "max","",       0, 0, 0, 0, ADC333_CHAN_LINE0),
    DSCR_X4RDS(AVG,,              "avg","",       0, 0, 0, 0, ADC333_CHAN_LINE0),
    DSCR_X4RDS(INT,,              "int","",       0, 0, 0, 0, ADC333_CHAN_LINE0),

    // LINE[0-3]TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC333_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                                \
    {N, "V", NULL,                                                    \
     __CX_CONCATENATE(ADC333_CHAN_LINE,x),                            \
     FASTADC_DATA_CN_MISSING,                                         \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC333_CHAN_LINE,x),ON),       \
     1000000.0,                                                       \
     {.int_r={ADC333_MIN_VALUE, ADC333_MAX_VALUE}},                   \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC333_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC333_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc333_line_dscrs[] =
{
    LINE_DSCR("A", 0),
    LINE_DSCR("B", 1),
    LINE_DSCR("C", 2),
    LINE_DSCR("D", 3),
};

pzframe_type_dscr_t *adc333_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc333",
                               0,
                               adc333_chan_dscrs, countof(adc333_chan_dscrs),
                               /* ADC characteristics */
                               ADC333_MAX_NUMPTS, ADC333_NUM_LINES,
                               ADC333_CHAN_CUR_NUMPTS,
                               ADC333_CHAN_CUR_PTSOFS,
                               ADC333_CHAN_XS_PER_POINT,
                               ADC333_CHAN_XS_DIVISOR,
                               ADC333_CHAN_XS_FACTOR,
                               adc333_info2mes,
                               adc333_line_dscrs,
                               /* Data specifics */
                               "Channel",
                               "% 6.3f",
                               adc333_x2xs, "us");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

#include "fastadc_data.h"
#include "adc200_data.h"
#include "drv_i/adc200_drv_i.h"


enum { ADC200_DTYPE      = CXDTYPE_INT32 };

static void adc200_info2mes(fastadc_data_t      *adc,
                            pzframe_chan_data_t *info,
                            fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < ADC200_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[ADC200_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[ADC200_CHAN_LINE1RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[ADC200_CHAN_LINE1RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[ADC200_CHAN_LINE1 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = ADC200_DTYPE;
    }

    mes_p->exttim = (info[ADC200_CHAN_TIMING].valbuf.i32 == ADC200_T_TIMER);
}

static int    adc200_x2xs(pzframe_chan_data_t *info, int x)
{
  static double timing_scales[4] = {1e9/200e6/*5.0*/, 1e9/195e6/*5.128*/, 1, 1};
  static int    frqdiv_scales[4] = {1, 2, 4, 8};

    x += info[ADC200_CHAN_CUR_PTSOFS].valbuf.i32;
    return (int)(x
                 * timing_scales[info[ADC200_CHAN_CUR_TIMING].valbuf.i32 & 3]
                 * frqdiv_scales[info[ADC200_CHAN_CUR_FRQDIV].valbuf.i32 & 3]);
}

static pzframe_chan_dscr_t adc200_chan_dscrs[] =
{
    [ADC200_CHAN_LINE1]        = {"line1", 0, PZFRAME_CHAN_IS_FRAME, 
                                  ADC200_DTYPE,
                                  ADC200_MAX_NUMPTS},
    [ADC200_CHAN_LINE2]        = {"line2", 0, PZFRAME_CHAN_IS_FRAME, 
                                  ADC200_DTYPE,
                                  ADC200_MAX_NUMPTS},
    [ADC200_CHAN_MARKER]       = {"marker",      0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC200_CHAN_SHOT]         = {"shot",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC200_CHAN_STOP]         = {"stop",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC200_CHAN_ISTART]       = {"istart"},
    [ADC200_CHAN_WAITTIME]     = {"waittime"},
    [ADC200_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC200_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC200_CHAN_NUMPTS]       = {"numpts"},
    [ADC200_CHAN_TIMING]       = {"timing"},
    [ADC200_CHAN_FRQDIV]       = {"frqdiv"},
    [ADC200_CHAN_RANGE1]       = {"range1"},
    [ADC200_CHAN_RANGE2]       = {"range2"},
    [ADC200_CHAN_ZERO1]        = {"zero1"},
    [ADC200_CHAN_ZERO2]        = {"zero2"},

    [ADC200_CHAN_ELAPSED]      = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC200_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [ADC200_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC200_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC200_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [ADC200_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [ADC200_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    [ADC200_CHAN_CUR_FRQDIV]   = {"cur_frqdiv",   1},
    [ADC200_CHAN_CUR_RANGE1]   = {"cur_range1",   1},
    [ADC200_CHAN_CUR_RANGE2]   = {"cur_range2",   1},
    [ADC200_CHAN_CUR_ZERO1]    = {"cur_zero1",    1},
    [ADC200_CHAN_CUR_ZERO2]    = {"cur_zero2",    1},

    [ADC200_CHAN_LINE1ON]       = {"line1on"},
    [ADC200_CHAN_LINE2ON]       = {"line2on"},
    [ADC200_CHAN_LINE1RANGEMIN] = {"line1rangemin"},
    [ADC200_CHAN_LINE2RANGEMIN] = {"line2rangemin"},
    [ADC200_CHAN_LINE1RANGEMAX] = {"line1rangemax"},
    [ADC200_CHAN_LINE2RANGEMAX] = {"line2rangemax"},

    [ADC200_CHAN_MIN1]          = {"min1"},
    [ADC200_CHAN_MIN2]          = {"min2"},
    [ADC200_CHAN_MAX1]          = {"max1"},
    [ADC200_CHAN_MAX2]          = {"max2"},
    [ADC200_CHAN_AVG1]          = {"avg1"},
    [ADC200_CHAN_AVG2]          = {"avg2"},
    [ADC200_CHAN_INT1]          = {"int1"},
    [ADC200_CHAN_INT2]          = {"int2"},

    // LINE{1,2}TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC200_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(ADC200_CHAN_LINE,x),               \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1000000.0,                                          \
     {.int_r={ADC200_MIN_VALUE, ADC200_MAX_VALUE}},      \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC200_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC200_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc200_line_dscrs[] =
{
    LINE_DSCR("A", 1),
    LINE_DSCR("B", 2),
};

pzframe_type_dscr_t *adc200_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc200",
                               0,
                               adc200_chan_dscrs, countof(adc200_chan_dscrs),
                               /* ADC characteristics */
                               ADC200_MAX_NUMPTS, ADC200_NUM_LINES,
                               ADC200_CHAN_CUR_NUMPTS,
                               ADC200_CHAN_CUR_PTSOFS,
                               ADC200_CHAN_XS_PER_POINT,
                               ADC200_CHAN_XS_DIVISOR,
                               ADC200_CHAN_XS_FACTOR,
                               adc200_info2mes,
                               adc200_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               "% 6.3f",
                               adc200_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

#include "fastadc_data.h"
#include "adc502_data.h"
#include "drv_i/adc502_drv_i.h"


enum { ADC502_DTYPE      = CXDTYPE_INT32 };

static void adc502_info2mes(fastadc_data_t      *adc,
                            pzframe_chan_data_t *info,
                            fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < ADC502_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[ADC502_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[ADC502_CHAN_LINE1RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[ADC502_CHAN_LINE1RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[ADC502_CHAN_LINE1 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = ADC502_DTYPE;
    }

    mes_p->exttim = (info[ADC502_CHAN_CUR_TIMING].valbuf.i32 == ADC502_TIMING_EXT);
}

static int    adc502_x2xs(pzframe_chan_data_t *info, int x)
{
    x +=   info[ADC502_CHAN_CUR_PTSOFS].valbuf.i32;
    return info[ADC502_CHAN_CUR_TIMING].valbuf.i32 == ADC502_TIMING_INT? x * 20 // Int.50MHz => 20ns/tick
                                                                       : x;     // else show as "1"/tick
}

static pzframe_chan_dscr_t adc502_chan_dscrs[] =
{
    [ADC502_CHAN_LINE1]        = {"line1", 0, PZFRAME_CHAN_IS_FRAME, 
                                  ADC502_DTYPE,
                                  ADC502_MAX_NUMPTS},
    [ADC502_CHAN_LINE2]        = {"line2", 0, PZFRAME_CHAN_IS_FRAME, 
                                  ADC502_DTYPE,
                                  ADC502_MAX_NUMPTS},
    [ADC502_CHAN_MARKER]       = {"marker",      0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC502_CHAN_SHOT]         = {"shot",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC502_CHAN_STOP]         = {"stop",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC502_CHAN_ISTART]       = {"istart"},
    [ADC502_CHAN_WAITTIME]     = {"waittime"},
    [ADC502_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC502_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC502_CHAN_NUMPTS]       = {"numpts"},
    [ADC502_CHAN_TIMING]       = {"timing"},
    [ADC502_CHAN_TMODE]        = {"tmode"},
    [ADC502_CHAN_RANGE1]       = {"range1"},
    [ADC502_CHAN_RANGE2]       = {"range2"},
    [ADC502_CHAN_SHIFT1]       = {"shift1"},
    [ADC502_CHAN_SHIFT2]       = {"shift2"},

    [ADC502_CHAN_ELAPSED]      = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC502_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [ADC502_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC502_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC502_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [ADC502_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [ADC502_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    [ADC502_CHAN_CUR_TMODE]    = {"cur_tmode",    1},
    [ADC502_CHAN_CUR_RANGE1]   = {"cur_range1",   1},
    [ADC502_CHAN_CUR_RANGE2]   = {"cur_range2",   1},
    [ADC502_CHAN_CUR_SHIFT1]   = {"cur_shift1",   1},
    [ADC502_CHAN_CUR_SHIFT2]   = {"cur_shift2",   1},

    [ADC502_CHAN_LINE1ON]       = {"line1on"},
    [ADC502_CHAN_LINE2ON]       = {"line2on"},
    [ADC502_CHAN_LINE1RANGEMIN] = {"line1rangemin"},
    [ADC502_CHAN_LINE2RANGEMIN] = {"line2rangemin"},
    [ADC502_CHAN_LINE1RANGEMAX] = {"line1rangemax"},
    [ADC502_CHAN_LINE2RANGEMAX] = {"line2rangemax"},

    [ADC502_CHAN_MIN1]          = {"min1"},
    [ADC502_CHAN_MIN2]          = {"min2"},
    [ADC502_CHAN_MAX1]          = {"max1"},
    [ADC502_CHAN_MAX2]          = {"max2"},
    [ADC502_CHAN_AVG1]          = {"avg1"},
    [ADC502_CHAN_AVG2]          = {"avg2"},
    [ADC502_CHAN_INT1]          = {"int1"},
    [ADC502_CHAN_INT2]          = {"int2"},

    // LINE{1,2}TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC502_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(ADC502_CHAN_LINE,x),               \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1000000.0,                                          \
     {.int_r={ADC502_MIN_VALUE, ADC502_MAX_VALUE}},      \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC502_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC502_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc502_line_dscrs[] =
{
    LINE_DSCR("A", 1),
    LINE_DSCR("B", 2),
};

pzframe_type_dscr_t *adc502_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc502",
                               0,
                               adc502_chan_dscrs, countof(adc502_chan_dscrs),
                               /* ADC characteristics */
                               ADC502_MAX_NUMPTS, ADC502_NUM_LINES,
                               ADC502_CHAN_CUR_NUMPTS,
                               ADC502_CHAN_CUR_PTSOFS,
                               ADC502_CHAN_XS_PER_POINT,
                               ADC502_CHAN_XS_DIVISOR,
                               ADC502_CHAN_XS_FACTOR,
                               adc502_info2mes,
                               adc502_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               "% 6.3f",
                               adc502_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

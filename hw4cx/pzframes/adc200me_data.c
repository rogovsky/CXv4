#include "fastadc_data.h"
#include "adc200me_data.h"
#include "drv_i/adc200me_drv_i.h"


enum { ADC200ME_DTYPE      = CXDTYPE_INT32 };

static void adc200me_info2mes(fastadc_data_t      *adc,
                              pzframe_chan_data_t *info,
                              fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < ADC200ME_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[ADC200ME_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[ADC200ME_CHAN_LINE1RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[ADC200ME_CHAN_LINE1RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[ADC200ME_CHAN_LINE1 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = ADC200ME_DTYPE;
    }

    mes_p->exttim = (info[ADC200ME_CHAN_CUR_TIMING].valbuf.i32 == ADC200ME_T_EXT);
}

static int    adc200me_x2xs(pzframe_chan_data_t *info, int x)
{
    x += info[ADC200ME_CHAN_CUR_PTSOFS].valbuf.i32;
    if (info[ADC200ME_CHAN_CUR_TIMING].valbuf.i32 == ADC200ME_T_200MHZ)
        return x * 5; // Int.200MHz => 5ns/tick
    else
        return x;     // else show as "1ns"/tick
}

static pzframe_chan_dscr_t adc200me_chan_dscrs[] =
{
    [ADC200ME_CHAN_LINE1]        = {"line1", 0, PZFRAME_CHAN_IS_FRAME, 
                                    ADC200ME_DTYPE,
                                    ADC200ME_MAX_NUMPTS},
    [ADC200ME_CHAN_LINE2]        = {"line2", 0, PZFRAME_CHAN_IS_FRAME, 
                                    ADC200ME_DTYPE,
                                    ADC200ME_MAX_NUMPTS},
    [ADC200ME_CHAN_MARKER]       = {"marker",      0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC200ME_CHAN_SHOT]         = {"shot",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC200ME_CHAN_STOP]         = {"stop",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC200ME_CHAN_ISTART]       = {"istart"},
    [ADC200ME_CHAN_WAITTIME]     = {"waittime"},
    [ADC200ME_CHAN_CALIBRATE]    = {"calibrate",   0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC200ME_CHAN_FGT_CLB]      = {"fgt_clb",     0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC200ME_CHAN_VISIBLE_CLB]  = {"visible_clb"},
    [ADC200ME_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC200ME_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC200ME_CHAN_NUMPTS]       = {"numpts"},
    [ADC200ME_CHAN_TIMING]       = {"timing"},
    [ADC200ME_CHAN_RANGE1]       = {"range1"},
    [ADC200ME_CHAN_RANGE2]       = {"range2"},

    [ADC200ME_CHAN_ELAPSED]      = {"elapsed",     0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC200ME_CHAN_SERIAL]       = {"serial",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC200ME_CHAN_CLB_STATE]    = {"clb_state"},
    [ADC200ME_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [ADC200ME_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC200ME_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC200ME_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [ADC200ME_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [ADC200ME_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    [ADC200ME_CHAN_CUR_RANGE1]   = {"cur_range1",   1},
    [ADC200ME_CHAN_CUR_RANGE2]   = {"cur_range2",   1},

    [ADC200ME_CHAN_LINE1ON]       = {"line1on"},
    [ADC200ME_CHAN_LINE2ON]       = {"line2on"},
    [ADC200ME_CHAN_LINE1RANGEMIN] = {"line1rangemin"},
    [ADC200ME_CHAN_LINE2RANGEMIN] = {"line2rangemin"},
    [ADC200ME_CHAN_LINE1RANGEMAX] = {"line1rangemax"},
    [ADC200ME_CHAN_LINE2RANGEMAX] = {"line2rangemax"},

    [ADC200ME_CHAN_CLB_OFS1]      = {"clb_ofs1"},
    [ADC200ME_CHAN_CLB_OFS2]      = {"clb_ofs2"},
    [ADC200ME_CHAN_CLB_COR1]      = {"clb_cor1"},
    [ADC200ME_CHAN_CLB_COR2]      = {"clb_cor2"},

    [ADC200ME_CHAN_MIN1]          = {"min1"},
    [ADC200ME_CHAN_MIN2]          = {"min2"},
    [ADC200ME_CHAN_MAX1]          = {"max1"},
    [ADC200ME_CHAN_MAX2]          = {"max2"},
    [ADC200ME_CHAN_AVG1]          = {"avg1"},
    [ADC200ME_CHAN_AVG2]          = {"avg2"},
    [ADC200ME_CHAN_INT1]          = {"int1"},
    [ADC200ME_CHAN_INT2]          = {"int2"},

    // LINE{1,2}TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC200ME_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(ADC200ME_CHAN_LINE,x),             \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1000000.0,                                          \
     {.int_r={ADC200ME_MIN_VALUE, ADC200ME_MAX_VALUE}},  \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC200ME_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC200ME_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc200me_line_dscrs[] =
{
    LINE_DSCR("A", 1),
    LINE_DSCR("B", 2),
};

pzframe_type_dscr_t *adc200me_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc200me",
                               0,
                               adc200me_chan_dscrs, countof(adc200me_chan_dscrs),
                               /* ADC characteristics */
                               ADC200ME_MAX_NUMPTS, ADC200ME_NUM_LINES,
                               ADC200ME_CHAN_CUR_NUMPTS,
                               ADC200ME_CHAN_CUR_PTSOFS,
                               ADC200ME_CHAN_XS_PER_POINT,
                               ADC200ME_CHAN_XS_DIVISOR,
                               ADC200ME_CHAN_XS_FACTOR,
                               adc200me_info2mes,
                               adc200me_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               "% 6.3f",
                               adc200me_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

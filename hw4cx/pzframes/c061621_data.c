#include "fastadc_data.h"
#include "c061621_data.h"
#include "drv_i/c061621_drv_i.h"


enum { C061621_DTYPE = CXDTYPE_INT32 };

static void c061621_info2mes(fastadc_data_t      *adc,
                             pzframe_chan_data_t *info,
                             fastadc_mes_t       *mes_p)
{
    mes_p->plots[0].on                 = 1;
    mes_p->plots[0].numpts             = info[C061621_CHAN_CUR_NUMPTS].valbuf.i32;
    mes_p->plots[0].cur_range.int_r[0] = info[C061621_CHAN_RANGEMIN].valbuf.i32;
    mes_p->plots[0].cur_range.int_r[1] = info[C061621_CHAN_RANGEMAX].valbuf.i32;
    FastadcSymmMinMaxInt(mes_p->plots[0].cur_range.int_r + 0,
                         mes_p->plots[0].cur_range.int_r + 1);
    mes_p->plots[0].cur_int_zero       = 0;

    mes_p->plots[0].x_buf              = info[C061621_CHAN_DATA].current_val;;
    mes_p->plots[0].x_buf_dtype        = C061621_DTYPE;

    mes_p->exttim = (info[C061621_CHAN_CUR_TIMING].valbuf.i32 == C061621_T_CPU  ||
                     info[C061621_CHAN_CUR_TIMING].valbuf.i32 == C061621_T_EXT);
}

static int    c061621_x2xs(pzframe_chan_data_t *info, int x)
{
  int        qv;

  static int q2xs[] =
  {
      50, 100, 200, 400, 500,
      1000, 2000, 4000, 5000, 10000, 20000, 40000, 50000,
      100000, 200000, 400000, 500000, 1000000, 2000000,
      1, 1
  };

    qv = 1;
    if (info[C061621_CHAN_CUR_TIMING].valbuf.i32 >= 0  &&
        info[C061621_CHAN_CUR_TIMING].valbuf.i32 < countof(q2xs))
        qv = q2xs[info[C061621_CHAN_CUR_TIMING].valbuf.i32];

    return (x + info[C061621_CHAN_CUR_PTSOFS].valbuf.i32) * qv;
}

static pzframe_chan_dscr_t c061621_chan_dscrs[] =
{
    [C061621_CHAN_DATA]         = {"data", 0, PZFRAME_CHAN_IS_FRAME | PZFRAME_CHAN_MARKER_MASK,
                                   C061621_DTYPE,
                                   C061621_MAX_NUMPTS},

    [C061621_CHAN_SHOT]         = {"shot",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [C061621_CHAN_STOP]         = {"stop",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [C061621_CHAN_ISTART]       = {"istart"},
    [C061621_CHAN_WAITTIME]     = {"waittime"},
    [C061621_CHAN_CALC_STATS]   = {"calc_stats"},

    [C061621_CHAN_PTSOFS]       = {"ptsofs"},
    [C061621_CHAN_NUMPTS]       = {"numpts"},
    [C061621_CHAN_TIMING]       = {"timing"},
    [C061621_CHAN_RANGE]        = {"range"},

    [C061621_CHAN_ELAPSED]      = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [C061621_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [C061621_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [C061621_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [C061621_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [C061621_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [C061621_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    [C061621_CHAN_CUR_RANGE]    = {"cur_range",    1},

    [C061621_CHAN_ON]           = {"on"},
    [C061621_CHAN_RANGEMIN]     = {"rangemin"},
    [C061621_CHAN_RANGEMAX]     = {"rangemax"},

    [C061621_CHAN_MIN]          = {"min"},
    [C061621_CHAN_MAX]          = {"max"},
    [C061621_CHAN_AVG]          = {"avg"},
    [C061621_CHAN_INT]          = {"int"},

    // TOTAL{MIN,MAX} and NUM_LINES are omitted

    [C061621_CHAN_DEVTYPE_ID]   = {"devtype_id"},
    [C061621_CHAN_DEVTYPE_STR]  = {"devtype_str", 0, PZFRAME_CHAN_IS_PARAM,
                                   CXDTYPE_TEXT, 20, -1},

//    [] = {""},
    [C061621_NUMCHANS] = {"_devstate"},
};

static fastadc_line_dscr_t c061621_line_dscrs[] =
{
    {"DATA", "V", NULL,
     C061621_CHAN_DATA,
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS,
     1000000.0,
     {.int_r={C061621_MIN_VALUE, C061621_MAX_VALUE}},
     C061621_CHAN_RANGEMIN, C061621_CHAN_RANGEMAX}
};

pzframe_type_dscr_t *c061621_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "c061621",
                               0,
                               c061621_chan_dscrs, countof(c061621_chan_dscrs),
                               /* ADC characteristics */
                               C061621_MAX_NUMPTS, C061621_NUM_LINES,
                               C061621_CHAN_CUR_NUMPTS,
                               C061621_CHAN_CUR_PTSOFS,
                               C061621_CHAN_XS_PER_POINT,
                               C061621_CHAN_XS_DIVISOR,
                               C061621_CHAN_XS_FACTOR,
                               c061621_info2mes,
                               c061621_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               "% 6.3f",
                               c061621_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

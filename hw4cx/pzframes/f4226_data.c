#include "fastadc_data.h"
#include "f4226_data.h"
#include "drv_i/f4226_drv_i.h"


enum { F4226_DTYPE = CXDTYPE_INT32 };

static void f4226_info2mes(fastadc_data_t      *adc,
                           pzframe_chan_data_t *info,
                           fastadc_mes_t       *mes_p)
{
    mes_p->plots[0].on                 = 1;
    mes_p->plots[0].numpts             = info[F4226_CHAN_CUR_NUMPTS].valbuf.i32;
    mes_p->plots[0].cur_range.int_r[0] = info[F4226_CHAN_RANGEMIN].valbuf.i32;
    mes_p->plots[0].cur_range.int_r[1] = info[F4226_CHAN_RANGEMAX].valbuf.i32;
    FastadcSymmMinMaxInt(mes_p->plots[0].cur_range.int_r + 0,
                         mes_p->plots[0].cur_range.int_r + 1);
    mes_p->plots[0].cur_int_zero       = 0;

    mes_p->plots[0].x_buf              = info[F4226_CHAN_DATA].current_val;;
    mes_p->plots[0].x_buf_dtype        = F4226_DTYPE;

    mes_p->exttim = (info[F4226_CHAN_CUR_TIMING].valbuf.i32 == F4226_T_EXT);
}

static int    f4226_x2xs(pzframe_chan_data_t *info, int x)
{
  int        qv;

  static int q2xs[] = {50, 100, 200, 400, 800, 1600, 3200, 1};

    qv = 1;
    if (info[F4226_CHAN_CUR_TIMING].valbuf.i32 >= 0  &&
        info[F4226_CHAN_CUR_TIMING].valbuf.i32 < countof(q2xs))
        qv = q2xs[info[F4226_CHAN_CUR_TIMING].valbuf.i32];

    return (x + 
            info[F4226_CHAN_CUR_PTSOFS   ].valbuf.i32 -
            info[F4226_CHAN_CUR_PREHIST64].valbuf.i32*64
           ) * qv;
}

static pzframe_chan_dscr_t f4226_chan_dscrs[] =
{
    [F4226_CHAN_DATA]          = {"data", 0, PZFRAME_CHAN_IS_FRAME | PZFRAME_CHAN_MARKER_MASK,
                                  F4226_DTYPE,
                                  F4226_MAX_NUMPTS},

    [F4226_CHAN_SHOT]          = {"shot",          0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [F4226_CHAN_STOP]          = {"stop",          0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [F4226_CHAN_ISTART]        = {"istart"},
    [F4226_CHAN_WAITTIME]      = {"waittime"},
    [F4226_CHAN_CALC_STATS]    = {"calc_stats"},

    [F4226_CHAN_PTSOFS]        = {"ptsofs"},
    [F4226_CHAN_NUMPTS]        = {"numpts"},
    [F4226_CHAN_TIMING]        = {"timing"},
    [F4226_CHAN_RANGE]         = {"range"},
    [F4226_CHAN_PREHIST64]     = {"prehist64"},

    [F4226_CHAN_ELAPSED]       = {"elapsed",       0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [F4226_CHAN_XS_PER_POINT]  = {"xs_per_point",  1},
    [F4226_CHAN_XS_DIVISOR]    = {"xs_divisor",    1},
    [F4226_CHAN_XS_FACTOR]     = {"xs_factor",     1},
    [F4226_CHAN_CUR_PTSOFS]    = {"cur_ptsofs",    1},
    [F4226_CHAN_CUR_NUMPTS]    = {"cur_numpts"},
    [F4226_CHAN_CUR_TIMING]    = {"cur_timing",    1},
    [F4226_CHAN_CUR_RANGE]     = {"cur_range",     1},
    [F4226_CHAN_CUR_PREHIST64] = {"cur_prehist64", 1},

    [F4226_CHAN_ON]            = {"on"},
    [F4226_CHAN_RANGEMIN]      = {"rangemin"},
    [F4226_CHAN_RANGEMAX]      = {"rangemax"},

    [F4226_CHAN_MIN]           = {"min"},
    [F4226_CHAN_MAX]           = {"max"},
    [F4226_CHAN_AVG]           = {"avg"},
    [F4226_CHAN_INT]           = {"int"},

    // TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [F4226_NUMCHANS] = {"_devstate"},
};

static fastadc_line_dscr_t f4226_line_dscrs[] =
{
    {"DATA", "V", NULL,
     F4226_CHAN_DATA,
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS,
     1000000.0,
     {.int_r={F4226_MIN_VALUE, F4226_MAX_VALUE}},
     F4226_CHAN_RANGEMIN, F4226_CHAN_RANGEMAX}
};

pzframe_type_dscr_t *f4226_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "f4226",
                               0,
                               f4226_chan_dscrs, countof(f4226_chan_dscrs),
                               /* ADC characteristics */
                               F4226_MAX_NUMPTS, F4226_NUM_LINES,
                               F4226_CHAN_CUR_NUMPTS,
                               F4226_CHAN_CUR_PTSOFS,
                               F4226_CHAN_XS_PER_POINT,
                               F4226_CHAN_XS_DIVISOR,
                               F4226_CHAN_XS_FACTOR,
                               f4226_info2mes,
                               f4226_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               "% 6.3f",
                               f4226_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

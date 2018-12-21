#include "fastadc_data.h"
#include "two812ch_data.h"
#include "two812ch_internals.h" // Is used instead of adc812me_drv_i.h


enum { TWO812CH_DTYPE      = CXDTYPE_INT32 };

static void two812ch_info2mes(fastadc_data_t      *adc,
                              pzframe_chan_data_t *info,
                              fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < TWO812CH_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[TWO812CH_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[TWO812CH_CHAN_LINE1RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[TWO812CH_CHAN_LINE1RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[TWO812CH_CHAN_LINE1 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = TWO812CH_DTYPE;
    }

    mes_p->exttim = (info[TWO812CH_CHAN_CUR_TIMING].valbuf.i32 == TWO812CH_T_EXT);
}

static int    two812ch_x2xs(pzframe_chan_data_t *info, int x)
{
  static double timing_scales[2] = {250, 1}; // That's in fact the "exttim" factor
  static int    frqdiv_scales[4] = {1, 2, 4, 8};

    x += info[TWO812CH_CHAN_CUR_PTSOFS].valbuf.i32;
    return (int)(x
                 * timing_scales[info[TWO812CH_CHAN_CUR_TIMING].valbuf.i32 & 1]
                 * frqdiv_scales[info[TWO812CH_CHAN_CUR_FRQDIV].valbuf.i32 & 3]);
}

static pzframe_chan_dscr_t two812ch_chan_dscrs[] =
{
    [TWO812CH_CHAN_LINE1]        = {"line1", 0, PZFRAME_CHAN_IS_FRAME, 
                                    TWO812CH_DTYPE,
                                    TWO812CH_MAX_NUMPTS},
    [TWO812CH_CHAN_LINE2]        = {"line2", 0, PZFRAME_CHAN_IS_FRAME, 
                                    TWO812CH_DTYPE,
                                    TWO812CH_MAX_NUMPTS},
    [TWO812CH_CHAN_MARKER]       = {"marker",      0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [TWO812CH_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [TWO812CH_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [TWO812CH_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [TWO812CH_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [TWO812CH_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [TWO812CH_CHAN_CUR_TIMING]   = {"cur_timing",   1},

    [TWO812CH_CHAN_LINE1RANGEMIN] = {"line1rangemin"},
    [TWO812CH_CHAN_LINE2RANGEMIN] = {"line2rangemin"},
    [TWO812CH_CHAN_LINE1RANGEMAX] = {"line1rangemax"},
    [TWO812CH_CHAN_LINE2RANGEMAX] = {"line2rangemax"},

    [TWO812CH_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(TWO812CH_CHAN_LINE,x),             \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1000000.0,                                          \
     {.int_r={TWO812CH_MIN_VALUE, TWO812CH_MAX_VALUE}},  \
     __CX_CONCATENATE(__CX_CONCATENATE(TWO812CH_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(TWO812CH_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t two812ch_line_dscrs[] =
{
    LINE_DSCR("U50kV", 1),
    LINE_DSCR("Irazm", 2),
};

pzframe_type_dscr_t *two812ch_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "two812ch",
                               PZFRAME_B_ARTIFICIAL | PZFRAME_B_NO_SVD | PZFRAME_B_NO_ALLOC,
                               two812ch_chan_dscrs, countof(two812ch_chan_dscrs),
                               /* ADC characteristics */
                               TWO812CH_MAX_NUMPTS, TWO812CH_NUM_LINES,
                               TWO812CH_CHAN_CUR_NUMPTS,
                               TWO812CH_CHAN_CUR_PTSOFS,
                               TWO812CH_CHAN_XS_PER_POINT,
                               TWO812CH_CHAN_XS_DIVISOR,
                               TWO812CH_CHAN_XS_FACTOR,
                               two812ch_info2mes,
                               two812ch_line_dscrs,
                               /* Data specifics */
                               "",
                               "% 6.3f",
                               two812ch_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

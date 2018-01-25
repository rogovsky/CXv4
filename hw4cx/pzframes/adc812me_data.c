#include "fastadc_data.h"
#include "adc812me_data.h"
#include "drv_i/adc812me_drv_i.h"


enum { ADC812ME_DTYPE = CXDTYPE_INT32 };

static void adc812me_info2mes(fastadc_data_t      *adc,
                              pzframe_chan_data_t *info,
                              fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < ADC812ME_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[ADC812ME_CHAN_CUR_NUMPTS].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[0] = info[ADC812ME_CHAN_LINE0RANGEMIN + nl].valbuf.i32;
        mes_p->plots[nl].cur_range.int_r[1] = info[ADC812ME_CHAN_LINE0RANGEMAX + nl].valbuf.i32;
        FastadcSymmMinMaxInt(mes_p->plots[nl].cur_range.int_r + 0,
                             mes_p->plots[nl].cur_range.int_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[ADC812ME_CHAN_LINE0 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = ADC812ME_DTYPE;
    }

    mes_p->exttim = (info[ADC812ME_CHAN_CUR_TIMING].valbuf.i32 == ADC812ME_T_EXT);
}

static int    adc812me_x2xs(pzframe_chan_data_t *info, int x)
{
  static double timing_scales[2] = {250, 1}; // That's in fact the "exttim" factor
  static int    frqdiv_scales[4] = {1, 2, 4, 8};

    x += info[ADC812ME_CHAN_CUR_PTSOFS].valbuf.i32;
    return (int)(x
                 * timing_scales[info[ADC812ME_CHAN_CUR_TIMING].valbuf.i32 & 1]
                 * frqdiv_scales[info[ADC812ME_CHAN_CUR_FRQDIV].valbuf.i32 & 3]);
}


#define DSCR_L1(n,pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    [__CX_CONCATENATE(ADC812ME_CHAN_,__CX_CONCATENATE(pfx,__CX_CONCATENATE(n,sfx)))] = {np __CX_STRINGIZE(n) ns, _change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn}
#define DSCR_X8(   pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems) \
    DSCR_L1(0, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(1, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(2, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(3, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(4, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(5, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(6, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(7, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1)
#define DSCR_X8RDS(pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    DSCR_L1(0, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+0), \
    DSCR_L1(1, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+1), \
    DSCR_L1(2, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+2), \
    DSCR_L1(3, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+3), \
    DSCR_L1(4, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+4), \
    DSCR_L1(5, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+5), \
    DSCR_L1(6, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+6), \
    DSCR_L1(7, pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn+7)

static pzframe_chan_dscr_t adc812me_chan_dscrs[] =
{
    DSCR_X8   (LINE,,               "line","",      0, PZFRAME_CHAN_IS_FRAME, 
                                                       ADC812ME_DTYPE,
                                                       ADC812ME_MAX_NUMPTS),
    [ADC812ME_CHAN_MARKER]       = {"marker",       0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC812ME_CHAN_SHOT]         = {"shot",         0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC812ME_CHAN_STOP]         = {"stop",         0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC812ME_CHAN_ISTART]       = {"istart"},
    [ADC812ME_CHAN_WAITTIME]     = {"waittime"},
    [ADC812ME_CHAN_CALIBRATE]    = {"calibrate",    0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC812ME_CHAN_FGT_CLB]      = {"fgt_clb",      0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC812ME_CHAN_VISIBLE_CLB]  = {"visible_clb"},
    [ADC812ME_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC812ME_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC812ME_CHAN_NUMPTS]       = {"numpts"},
    [ADC812ME_CHAN_TIMING]       = {"timing"},
    [ADC812ME_CHAN_FRQDIV]       = {"frqdiv"},
    DSCR_X8   (RANGE,,              "range","",     0, 0, 0, 0),

    [ADC812ME_CHAN_ELAPSED]      = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC812ME_CHAN_SERIAL]       = {"serial",       0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC812ME_CHAN_CLB_STATE]    = {"clb_state"},
    [ADC812ME_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [ADC812ME_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC812ME_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC812ME_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [ADC812ME_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [ADC812ME_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    [ADC812ME_CHAN_CUR_FRQDIV]   = {"cur_frqdiv",   1},
    DSCR_X8   (CUR_RANGE,,          "cur_range","", 1, 0, 0, 0),

    DSCR_X8   (LINE,ON,          "line","on",       0, 0, 0, 0),
    DSCR_X8RDS(LINE,RANGEMIN,    "line","rangemin", 0, 0, 0, 0, ADC812ME_CHAN_LINE0),
    DSCR_X8RDS(LINE,RANGEMAX,    "line","rangemax", 0, 0, 0, 0, ADC812ME_CHAN_LINE0),

    DSCR_X8   (CLB_OFS,,            "clb_ofs","",   0, 0, 0, 0),
    DSCR_X8   (CLB_COR,,            "clb_cor","",   0, 0, 0, 0),

    DSCR_X8RDS(MIN,,                "min","",       0, 0, 0, 0, ADC812ME_CHAN_LINE0),
    DSCR_X8RDS(MAX,,                "max","",       0, 0, 0, 0, ADC812ME_CHAN_LINE0),
    DSCR_X8RDS(AVG,,                "avg","",       0, 0, 0, 0, ADC812ME_CHAN_LINE0),
    DSCR_X8RDS(INT,,                "int","",       0, 0, 0, 0, ADC812ME_CHAN_LINE0),

    // LINE[0-7]TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC812ME_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(ADC812ME_CHAN_LINE,x),             \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1000000.0,                                          \
     {.int_r={ADC812ME_MIN_VALUE, ADC812ME_MAX_VALUE}},  \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC812ME_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC812ME_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc812me_line_dscrs[] =
{
    LINE_DSCR("0A", 0),
    LINE_DSCR("0B", 1),
    LINE_DSCR("1A", 2),
    LINE_DSCR("1B", 3),
    LINE_DSCR("2A", 4),
    LINE_DSCR("2B", 5),
    LINE_DSCR("3A", 6),
    LINE_DSCR("3B", 7),
};

pzframe_type_dscr_t *adc812me_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc812me",
                               0,
                               adc812me_chan_dscrs, countof(adc812me_chan_dscrs),
                               /* ADC characteristics */
                               ADC812ME_MAX_NUMPTS, ADC812ME_NUM_LINES,
                               ADC812ME_CHAN_CUR_NUMPTS,
                               ADC812ME_CHAN_CUR_PTSOFS,
                               ADC812ME_CHAN_XS_PER_POINT,
                               ADC812ME_CHAN_XS_DIVISOR,
                               ADC812ME_CHAN_XS_FACTOR,
                               adc812me_info2mes,
                               adc812me_line_dscrs,
                               /* Data specifics */
                               "Channel",
                               "% 6.3f",
                               adc812me_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

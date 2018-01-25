#include "fastadc_data.h"
#include "adc4x250_data.h"
#include "drv_i/adc4x250_drv_i.h"


enum { ADC4X250_DTYPE      = CXDTYPE_INT32 };

static void adc4x250_info2mes(fastadc_data_t      *adc,
                              pzframe_chan_data_t *info,
                              fastadc_mes_t       *mes_p)
{
}

static int    adc4x250_x2xs(pzframe_chan_data_t *info, int x)
{
    return x;
}

#define DSCR_L1(n,pfx,sfx,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    [__CX_CONCATENATE(ADC4X250_CHAN_,__CX_CONCATENATE(pfx,__CX_CONCATENATE(n,sfx)))] = {np __CX_STRINGIZE(n) ns, _change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn}
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

static pzframe_chan_dscr_t adc4x250_chan_dscrs[] =
{
    DSCR_X4   (LINE,,             "line","",      0, PZFRAME_CHAN_IS_FRAME, 
                                                     ADC4X250_DTYPE,
                                                     ADC4X250_MAX_NUMPTS),
    [ADC4X250_CHAN_MARKER]       = {"marker",       0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [ADC4X250_CHAN_SHOT]         = {"shot",         0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC4X250_CHAN_STOP]         = {"stop",         0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC4X250_CHAN_ISTART]       = {"istart"},
    [ADC4X250_CHAN_WAITTIME]     = {"waittime"},
    [ADC4X250_CHAN_CALIBRATE]    = {"calibrate",    0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC4X250_CHAN_FGT_CLB]      = {"fgt_clb",      0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [ADC4X250_CHAN_VISIBLE_CLB]  = {"visible_clb"},
    [ADC4X250_CHAN_CALC_STATS]   = {"calc_stats"},

    [ADC4X250_CHAN_PTSOFS]       = {"ptsofs"},
    [ADC4X250_CHAN_NUMPTS]       = {"numpts"},
    [ADC4X250_CHAN_TIMING]       = {"timing"},
    [ADC4X250_CHAN_FRQDIV]       = {"frqdiv"},
    DSCR_X4   (RANGE,,              "range","",     0, 0, 0, 0),
    [ADC4X250_CHAN_TRIG_TYPE]    = {"trig_type"},
    [ADC4X250_CHAN_TRIG_INPUT]   = {"trig_input"},

    [ADC4X250_CHAN_DEVICE_ID]    = {"device_id",    0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_BASE_SW_VER]  = {"base_sw_ver",  0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_PGA_SW_VER]   = {"pga_sw_ver",   0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_BASE_HW_VER]  = {"base_hw_ver",  0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_PGA_HW_VER]   = {"pga_hw_ver",   0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_PGA_VAR]      = {"pga_var",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_BASE_UNIQ_ID] = {"base_uniq_id", 0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_PGA_UNIQ_ID]  = {"pga_uniq_id",  0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},

    [ADC4X250_CHAN_ELAPSED]      = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},
    [ADC4X250_CHAN_CLB_STATE]    = {"clb_state"},
    [ADC4X250_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [ADC4X250_CHAN_XS_DIVISOR]   = {"xs_divisor",   1},
    [ADC4X250_CHAN_XS_FACTOR]    = {"xs_factor",    1},
    [ADC4X250_CHAN_CUR_PTSOFS]   = {"cur_ptsofs",   1},
    [ADC4X250_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [ADC4X250_CHAN_CUR_TIMING]   = {"cur_timing",   1},
    [ADC4X250_CHAN_CUR_FRQDIV]   = {"cur_frqdiv",   1},
    DSCR_X4   (CUR_RANGE,,          "cur_range","", 1, 0, 0, 0),
    DSCR_X4   (OVERFLOW,,           "overflow","",  0, 0, 0, 0),

    DSCR_X4   (LINE,ON,          "line","on",       0, 0, 0, 0),
    DSCR_X4RDS(LINE,RANGEMIN,    "line","rangemin", 0, 0, 0, 0, ADC4X250_CHAN_LINE0),
    DSCR_X4RDS(LINE,RANGEMAX,    "line","rangemax", 0, 0, 0, 0, ADC4X250_CHAN_LINE0),

    DSCR_X4   (CLB_FAIL,,           "clb_fail","",  0, 0, 0, 0),
    DSCR_X4   (CLB_DYN,,            "clb_dyn", "",  0, 0, 0, 0),
    DSCR_X4   (CLB_ZERO,,           "clb_zero","",  0, 0, 0, 0),
    DSCR_X4   (CLB_GAIN,,           "clb_gain","",  0, 0, 0, 0),

    DSCR_X4RDS(MIN,,                "min","",       0, 0, 0, 0, ADC4X250_CHAN_LINE0),
    DSCR_X4RDS(MAX,,                "max","",       0, 0, 0, 0, ADC4X250_CHAN_LINE0),
    DSCR_X4RDS(AVG,,                "avg","",       0, 0, 0, 0, ADC4X250_CHAN_LINE0),
    DSCR_X4RDS(INT,,                "int","",       0, 0, 0, 0, ADC4X250_CHAN_LINE0),

    // LINE[0-3]TOTAL{MIN,MAX} and NUM_LINES are omitted

//    [] = {""},
    [ADC4X250_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                                  \
    {N, "V", NULL,                                                      \
     __CX_CONCATENATE(ADC4X250_CHAN_LINE,x),                            \
     FASTADC_DATA_CN_MISSING,                                           \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC4X250_CHAN_LINE,x),ON),       \
     1000000.0,                                                         \
     {.int_r={ADC4X250_MIN_VALUE, ADC4X250_MAX_VALUE}},                 \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC4X250_CHAN_LINE,x),RANGEMIN), \
     __CX_CONCATENATE(__CX_CONCATENATE(ADC4X250_CHAN_LINE,x),RANGEMAX)}
static fastadc_line_dscr_t adc4x250_line_dscrs[] =
{
    LINE_DSCR("A", 0),
    LINE_DSCR("B", 1),
    LINE_DSCR("C", 2),
    LINE_DSCR("D", 3),
};

pzframe_type_dscr_t *adc4x250_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "adc4x250",
                               0,
                               adc4x250_chan_dscrs, countof(adc4x250_chan_dscrs),
                               /* ADC characteristics */
                               ADC4X250_MAX_NUMPTS, ADC4X250_NUM_LINES,
                               ADC4X250_CHAN_CUR_NUMPTS,
                               ADC4X250_CHAN_CUR_PTSOFS,
                               ADC4X250_CHAN_XS_PER_POINT,
                               ADC4X250_CHAN_XS_DIVISOR,
                               ADC4X250_CHAN_XS_FACTOR,
                               adc4x250_info2mes,
                               adc4x250_line_dscrs,
                               /* Data specifics */
                               "Channel",
                               "% 6.3f",
                               adc4x250_x2xs, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

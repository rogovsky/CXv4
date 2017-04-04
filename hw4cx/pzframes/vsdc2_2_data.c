#include "fastadc_data.h"
#include "vsdc2_2_data.h"
#include "drv_i/vsdc2_drv_i.h"


enum { VSDC2_DTYPE      = CXDTYPE_SINGLE };

static void vsdc2_2_info2mes(fastadc_data_t      *adc,
                             pzframe_chan_data_t *info,
                             fastadc_mes_t       *mes_p)
{
  int               nl;

    for (nl = 0;  nl < VSDC2_NUM_LINES;  nl++)
    {
        mes_p->plots[nl].on                 = 1;
        mes_p->plots[nl].numpts             = info[VSDC2_CHAN_CUR_NUMPTS0+nl].valbuf.i32;
//        mes_p->plots[nl].cur_range.dbl_r[0] = VSDC2_MIN_VALUE;
//        mes_p->plots[nl].cur_range.dbl_r[1] = VSDC2_MAX_VALUE;
//        FastadcSymmMinMaxDbl(mes_p->plots[nl].cur_range.dbl_r + 0,
//                             mes_p->plots[nl].cur_range.dbl_r + 1);
        mes_p->plots[nl].cur_int_zero       = 0;

        mes_p->plots[nl].x_buf              = info[VSDC2_CHAN_LINE0 + nl].current_val;
        mes_p->plots[nl].x_buf_dtype        = VSDC2_DTYPE;
    }

    mes_p->exttim = 0;
}

static int    vsdc2_2_x2xs(pzframe_chan_data_t *info, int x)
{
    return (x + info[VSDC2_CHAN_CUR_PTSOFS0].valbuf.i32)*32/10;
}

static pzframe_chan_dscr_t vsdc2_2_chan_dscrs[] =
{
    [VSDC2_CHAN_LINE0]        = {"line0", 0, PZFRAME_CHAN_IS_FRAME | PZFRAME_CHAN_MARKER_MASK,
                                 VSDC2_DTYPE,
                                 VSDC2_MAX_NUMPTS},
    [VSDC2_CHAN_LINE1]        = {"line1", 0, PZFRAME_CHAN_IS_FRAME | PZFRAME_CHAN_MARKER_MASK,
                                 VSDC2_DTYPE,
                                 VSDC2_MAX_NUMPTS},

    [VSDC2_CHAN_INT_n_base + 0] = {"int0", 0, 0, VSDC2_DTYPE},
    [VSDC2_CHAN_INT_n_base + 1] = {"int1", 0, 0, VSDC2_DTYPE},

    [VSDC2_CHAN_GAIN_n_base + 0] = {"gain0"},
    [VSDC2_CHAN_GAIN_n_base + 1] = {"gain1"},

    [VSDC2_CHAN_GET_OSC0]     = {"get_osc0", 0, PZFRAME_CHAN_IMMEDIATE_MASK},
    [VSDC2_CHAN_GET_OSC1]     = {"get_osc1", 0, PZFRAME_CHAN_IMMEDIATE_MASK},

    [VSDC2_CHAN_PTSOFS0]      = {"ptsofs0"},
    [VSDC2_CHAN_PTSOFS1]      = {"ptsofs1"},
//    [VSDC2_CHAN_NUMPTS0]      = {"numpts0"},
//    [VSDC2_CHAN_NUMPTS1]      = {"numpts1"},

    [VSDC2_CHAN_CUR_PTSOFS0]  = {"cur_ptsofs0",   1},
    [VSDC2_CHAN_CUR_PTSOFS1]  = {"cur_ptsofs1",   1},
    [VSDC2_CHAN_CUR_NUMPTS0]  = {"cur_numpts0"},
    [VSDC2_CHAN_CUR_NUMPTS1]  = {"cur_numpts1"},
    [VSDC2_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(N,x)                                   \
    {N, "V", NULL,                                       \
     __CX_CONCATENATE(VSDC2_CHAN_LINE,x),                \
     __CX_CONCATENATE(VSDC2_CHAN_CUR_NUMPTS,x), FASTADC_DATA_IS_ON_ALWAYS, \
     1.0,                                          \
     {.dbl_r={VSDC2_MIN_VALUE*0, VSDC2_MAX_VALUE*0}},        \
      FASTADC_DATA_CN_MISSING, FASTADC_DATA_CN_MISSING}
static fastadc_line_dscr_t vsdc2_2_line_dscrs[] =
{
    LINE_DSCR("0", 0),
    LINE_DSCR("1", 1),
};

pzframe_type_dscr_t *vsdc2_2_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "vsdc2_2",
                               0,
                               vsdc2_2_chan_dscrs, countof(vsdc2_2_chan_dscrs),
                               /* ADC characteristics */
                               VSDC2_MAX_NUMPTS, VSDC2_NUM_LINES,
                               FASTADC_DATA_CN_MISSING,
                               VSDC2_CHAN_CUR_PTSOFS0,
                               -3200,
                               FASTADC_DATA_CN_MISSING,
                               -9,
                               vsdc2_2_info2mes,
                               vsdc2_2_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               /*"% 6.3f"*/"%11.4e",
                               vsdc2_2_x2xs, "ms");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

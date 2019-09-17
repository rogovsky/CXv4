#include "fastadc_data.h"
#include "pxi6363_data.h"
#include "drv_i/pxi6363_drv_i.h"


enum { PXI6363_DTYPE = CXDTYPE_DOUBLE };


#define DSCR_L1(n,base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn) \
    [__CX_CONCATENATE(PXI6363_CHAN_,base)+n] = {np __CX_STRINGIZE(n) ns, _change_important,_chan_type,_dtype,_max_nelems,_rd_source_cn}
#define DSCR_X16( base,np,ns,_change_important,_chan_type,_dtype,_max_nelems) \
    DSCR_L1(0,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(1,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(2,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(3,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(4,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(5,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(6,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(7,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(8,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(9,  base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(10, base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(11, base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(12, base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(13, base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(14, base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1), \
    DSCR_L1(15, base,np,ns,_change_important,_chan_type,_dtype,_max_nelems,-1)

static pzframe_chan_dscr_t pxi6363_chan_dscrs[] =
{
    DSCR_X16(OSC_n_base,           "adc_osc","",   0, PZFRAME_CHAN_IS_FRAME,
                                                      PXI6363_DTYPE,
                                                      PXI6363_MAX_NUMPTS),

    [PXI6363_CHAN_MARKER]       = {"marker",       0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_MARKER_MASK},

    [PXI6363_CHAN_NUMPTS]       = {"numpts"},
    [PXI6363_CHAN_XS_PER_POINT] = {"xs_per_point", 1},
    [PXI6363_CHAN_CUR_NUMPTS]   = {"cur_numpts"},
    [PXI6363_CHAN_ALL_RANGEMIN] = {"all_rangemin", 0, 0, CXDTYPE_DOUBLE},
    [PXI6363_CHAN_ALL_RANGEMAX] = {"all_rangemax", 0, 0, CXDTYPE_DOUBLE},

    [PXI6363_NUMCHANS] = {"_devstate"},
};

#define LINE_DSCR(n)                                     \
    {__CX_STRINGIZE(n), "V", NULL,                       \
     PXI6363_CHAN_OSC_n_base + n,                        \
     FASTADC_DATA_CN_MISSING, FASTADC_DATA_IS_ON_ALWAYS, \
     1.0,                                                \
     {.dbl_r={PXI6363_MIN_VALUE, PXI6363_MAX_VALUE}},    \
     PXI6363_CHAN_ALL_RANGEMIN,                          \
     PXI6363_CHAN_ALL_RANGEMAX}
static fastadc_line_dscr_t pxi6363_line_dscrs[] =
{
    LINE_DSCR(0),
    LINE_DSCR(1),
    LINE_DSCR(2),
    LINE_DSCR(3),
    LINE_DSCR(4),
    LINE_DSCR(5),
    LINE_DSCR(6),
    LINE_DSCR(7),
    LINE_DSCR(8),
    LINE_DSCR(9),
    LINE_DSCR(10),
    LINE_DSCR(11),
    LINE_DSCR(12),
    LINE_DSCR(13),
    LINE_DSCR(14),
    LINE_DSCR(15),
};

pzframe_type_dscr_t *pxi6363_get_type_dscr(void)
{
  static fastadc_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        FastadcDataFillStdDscr(&dscr, "pxi6363",
                               0,
                               pxi6363_chan_dscrs, countof(pxi6363_chan_dscrs),
                               /* ADC characteristics */
                               PXI6363_MAX_NUMPTS, PXI6363_NUM_LINES,
                               PXI6363_CHAN_CUR_NUMPTS,
                               -1,
                               /*!!!PXI6363_CHAN_XS_PER_POINT*/-1,
                               -1,
                               -1,
                               NULL,
                               pxi6363_line_dscrs,
                               /* Data specifics */
                               "Ch",
                               "% 6.3f",
                               NULL, "ns");
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

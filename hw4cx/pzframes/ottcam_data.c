#include "vcamimg_data.h"
#include "ottcam_data.h"
#include "drv_i/ottcam_drv_i.h"


enum { OTTCAM_DTYPE = CXDTYPE_UINT16 };

static pzframe_chan_dscr_t ottcam_chan_dscrs[] =
{
    [OTTCAM_CHAN_DATA]         = {"data",        0, PZFRAME_CHAN_IS_FRAME | PZFRAME_CHAN_MARKER_MASK,
                                  OTTCAM_DTYPE,
                                  OTTCAM_MAX_W*OTTCAM_MAX_H},

    [OTTCAM_CHAN_SHOT]         = {"shot",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [OTTCAM_CHAN_STOP]         = {"stop",        0, PZFRAME_CHAN_IS_PARAM | PZFRAME_CHAN_RW_ONLY_MASK},
    [OTTCAM_CHAN_ISTART]       = {"istart"},
    [OTTCAM_CHAN_WAITTIME]     = {"waittime"},

    [OTTCAM_CHAN_X]             = {"x"},
    [OTTCAM_CHAN_Y]             = {"y"},
    [OTTCAM_CHAN_W]             = {"w"},
    [OTTCAM_CHAN_H]             = {"h"},
    [OTTCAM_CHAN_K]             = {"k"},
    [OTTCAM_CHAN_T]             = {"t"},
    [OTTCAM_CHAN_SYNC]          = {"sync"},
    [OTTCAM_CHAN_RRQ_MSECS]     = {"rrq_msecs"},

    [OTTCAM_CHAN_MISS]          = {"miss"},
    [OTTCAM_CHAN_ELAPSED]       = {"elapsed",      0, PZFRAME_CHAN_IMMEDIATE_MASK | PZFRAME_CHAN_ON_CYCLE_MASK},

    [OTTCAM_CHAN_CUR_X]         = {"cur_x"},
    [OTTCAM_CHAN_CUR_Y]         = {"cur_y"},
    [OTTCAM_CHAN_CUR_W]         = {"cur_w"},
    [OTTCAM_CHAN_CUR_H]         = {"cur_h"},
    [OTTCAM_CHAN_CUR_K]         = {"cur_k"},
    [OTTCAM_CHAN_CUR_T]         = {"cur_t"},
    [OTTCAM_CHAN_CUR_SYNC]      = {"cur_sync"},
    [OTTCAM_CHAN_CUR_RRQ_MSECS] = {"cur_rrq_msecs"},

//    [] = {""},
    [OTTCAM_NUMCHANS] = {"_devstate"},
};

pzframe_type_dscr_t *ottcam_get_type_dscr(void)
{
  static vcamimg_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        VcamimgDataFillStdDscr(&dscr, "ottcam",
                               0,
                               ottcam_chan_dscrs, countof(ottcam_chan_dscrs),
                               /* Image characteristics */
                               OTTCAM_CHAN_DATA,
                               OTTCAM_MAX_W,   OTTCAM_MAX_H,
                               OTTCAM_MAX_W-1, OTTCAM_MAX_H,
                               0,              0,
                               OTTCAM_BPP,
                               OTTCAM_SRCMAXVAL);
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

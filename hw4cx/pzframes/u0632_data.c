#include "wirebpm_data.h"
#include "u0632_data.h"
#include "drv_i/u0632_drv_i.h"
#include "u0632_internals.h"


enum {U0632_DTYPE = CXDTYPE_INT32};

static pzframe_chan_dscr_t u0632_chan_dscrs[] =
{
    [C_DATA]   = {"data", 0, PZFRAME_CHAN_IS_FRAME | PZFRAME_CHAN_MARKER_MASK,
                  U0632_DTYPE,
                  U0632_CELLS_PER_UNIT},
    [C_ONLINE] = {"online"},
    [C_CUR_M]  = {"cur_m"},
    [C_CUR_P]  = {"cur_p"},
    [C_M]      = {"m"},
    [C_P]      = {"p"},
};

pzframe_type_dscr_t *u0632_get_type_dscr(void)
{
  static wirebpm_type_dscr_t  dscr;
  static int                  dscr_inited;

    if (!dscr_inited)
    {
        WirebpmDataFillStdDscr(&dscr, "u0632",
                               0,
                               u0632_chan_dscrs, countof(u0632_chan_dscrs),
                               /* Data characteristics */
                               C_DATA);
        dscr_inited = 1;
    }
    return &(dscr.ftd);
}

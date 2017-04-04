/*
 *  List of symbols which aren't used by drvinfo,
 *  but need to be present for drivers.
 */


#include "misclib.h"
#include "cxsd_driver.h"
#include "sendqlib.h"


void *public_funcs_list [] =
{
    set_fd_flags,
    DoDriverLog,
    sq_init,
};

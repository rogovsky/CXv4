#include "cxsd_driver.h"


static int  noop_init_d(int devid, void *devptr,
                        int businfocount, int businfo[],
                        const char *auxinfo)
{

    DoDriverLog(devid, DRIVERLOG_NOTICE, "%s([%d], <%s>:\"%s\")",
                __FUNCTION__, businfocount, GetDevTypename(devid), auxinfo);
    return DEVSTATE_OPERATING;
}

static void noop_term_d(int devid, void *devptr)
{
    DoDriverLog(devid, DRIVERLOG_NOTICE, "%s()", __FUNCTION__);
}

DEFINE_CXSD_DRIVER(noop, "NO-OP driver, for tests and simulation",
                   NULL, NULL,
                   0, NULL,
                   0, 1000,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   noop_init_d, noop_term_d, StdSimulated_rw_p);

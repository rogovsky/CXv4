#include "cxsd_driver.h"


static int  test_rd_init_d(int devid, void *devptr,
                           int businfocount, int businfo[],
                           const char *auxinfo)
{

    DoDriverLog(devid, DRIVERLOG_NOTICE, "%s([%d], <%s>:\"%s\")",
                __FUNCTION__, businfocount, GetDevTypename(devid), auxinfo);
    return DEVSTATE_OPERATING;
}

static void test_rd_term_d(int devid, void *devptr)
{
    DoDriverLog(devid, DRIVERLOG_NOTICE, "%s()", __FUNCTION__);
}

// w3i {result:0; input:1; coeff:2}
static void test_rd_rw_p(int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        if      (chn == 0)
        {
            ReturnInt32Datum    (devid, chn, 0, 0);
        }
        else if (chn == 1)
        {
            if (action == DRVA_READ)
                ReturnInt32Datum(devid, chn, 0, 0);
            else
            {
                ReturnInt32Datum(devid, chn, val, 0);
                ReturnInt32Datum(devid, 0,   val, 0);
            }
        }
        else
        {
            if (action == DRVA_READ)
                ReturnInt32Datum(devid, chn, 0,   0);
            else
            {
                ReturnInt32Datum(devid, chn, val, 0);
                SetChanRDs      (devid, 0, 1, val, 0);
            }
        }
 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(test_rd, "NO-OP driver, for tests and simulation",
                   NULL, NULL,
                   0, NULL,
                   0, 1000,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   test_rd_init_d, test_rd_term_d, test_rd_rw_p);

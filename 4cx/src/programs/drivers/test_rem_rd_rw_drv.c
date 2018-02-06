#define DEFINE_CXSD_DRIVER(name, comment,                           \
                           init_mod, term_mod,                      \
                           privrecsize, paramtable,                 \
                           min_businfo_n, max_businfo_n,            \
                           layer, layerver,                         \
                           extensions,                              \
                           chan_nsegs, chan_info, chan_namespace,   \
                           init_dev, term_dev, rdwr_p)              \
    CxsdDriverModRec CXSD_DRIVER_MODREC_NAME(name) =                \
    {                                                               \
        {                                                           \
            CXSD_DRIVER_MODREC_MAGIC, CXSD_DRIVER_MODREC_VERSION,   \
            __CX_STRINGIZE(name), comment,                          \
            init_mod, term_mod                                      \
        },                                                          \
        privrecsize, paramtable,                                    \
        min_businfo_n, max_businfo_n,                               \
        layer, layerver,                                            \
        extensions,                                                 \
        chan_nsegs, chan_info, chan_namespace,                      \
        init_dev, term_dev,                                         \
        rdwr_p                                                      \
    };                                                              \
    int main(void)                                                  \
    {                                                               \
      static int8  privrec[privrecsize];                            \
                                                                    \
        return remdrvlet_main(&CXSD_DRIVER_MODREC_NAME(name),       \
                              (privrecsize) != 0? privrec : NULL);  \
    }
#define TRUE_DEFINE_DRIVER_H_FILE "/dev/null"
#include "cxsd_driver.h"


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int32 v;
} privrec_t;

static int  test_rem_rd_rw_init_d(int devid, void *devptr,
                                  int businfocount, int businfo[],
                                  const char *auxinfo)
{
    SetChanRDs(devid, 0, 1, 1000000.0, 0);
    return DEVSTATE_OPERATING;
}

static void test_rem_rd_rw_rw_p(int devid, void *devptr,
                                int action,
                                int count, int *addrs,
                                cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t          *me = devptr;

  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
fprintf(stderr, "%s %c#%d\n", __FUNCTION__, action==DRVA_WRITE?'w':'r', chn);
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        if (action == DRVA_WRITE) me->v = val;
        ReturnInt32Datum(devid, chn, me->v, 0);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(rem_rd_rw, "Remote {R,D} test driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   test_rem_rd_rw_init_d, NULL, test_rem_rd_rw_rw_p);

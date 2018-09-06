#include "cxsd_driver.h"

#include "drv_i/frolov_k4x1_drv_i.h"


typedef struct
{
    int       N;
} frolov_k4x1_privrec_t;

static int frolov_k4x1_init_d(int devid, void *devptr,
                              int businfocount, int *businfo,
                              const char *auxinfo)
{
  frolov_k4x1_privrec_t *me = (frolov_k4x1_privrec_t*)devptr;

    me->N = businfo[0];

    return DEVSTATE_OPERATING;
}

static void frolov_k4x1_rw_p(int devid, void *devptr,
                             int action,
                             int count, int *addrs,
                             cxdtype_t *dtypes, int *nelems, void **values)
{
  frolov_k4x1_privrec_t *me = (frolov_k4x1_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel

  int32            value;
  rflags_t         rflags;

  int              w;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            value = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            value = 0xFFFFFFFF; // That's just to make GCC happy

        if      (chn == FROLOV_K4X1_CHAN_IN_SEL)
        {
            rflags = 0;
            if (action == DRVA_WRITE)
            {
                if (value < 0) value = 0;
                if (value > 4) value = 4;
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                w = (w &~ 7) | (value);
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &w));
            }
            rflags     |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
            value = w & 7;
        }
        else if (chn == FROLOV_K4X1_CHAN_OUT_SEL)
        {
            rflags = 0;
            if (action == DRVA_WRITE)
            {
                if (value < 0) value = 0;
                if (value > 3) value = 3;
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                w = (w &~ (3 << 3)) | (value << 3);
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &w));
            }
            rflags     |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
            value = (w >> 3) & 3;
        }
        else
        {
            value   = 0;
            rflags  = CXRF_UNSUPPORTED;
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(frolov_k4x1, "Frolov K4X1",
                   NULL, NULL,
                   sizeof(frolov_k4x1_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   frolov_k4x1_init_d, NULL, frolov_k4x1_rw_p);

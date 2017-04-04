#include "cxsd_driver.h"

#include "drv_i/frolov_bfp16_drv_i.h"


typedef struct
{
    int  devid;
    int  N;
} frolov_bfp16_privrec_t;


static void frolov_bfp16_rw_p(int devid, void *devptr,
                              int action,
                              int count, int *addrs,
                              cxdtype_t *dtypes, int *nelems, void **values);

static void ReturnOne(frolov_bfp16_privrec_t *me, int nc)
{
    frolov_bfp16_rw_p(me->devid, me,
                      DRVA_READ,
                      1, &nc,
                      NULL, NULL, NULL);
}

static int frolov_bfp16_init_d(int devid, void *devptr,
                               int businfocount, int *businfo,
                               const char *auxinfo)
{
  frolov_bfp16_privrec_t *me = (frolov_bfp16_privrec_t*)devptr;
  
    me->devid   = devid;
    me->N       = businfo[0];

    return DEVSTATE_OPERATING;
}

static void frolov_bfp16_rw_p(int devid, void *devptr,
                              int action,
                              int count, int *addrs,
                              cxdtype_t *dtypes, int *nelems, void **values)
{
  frolov_bfp16_privrec_t *me = (frolov_bfp16_privrec_t*)devptr;

  int                     n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int                     chn;   // channel
  
  int                     c;
  int32                   value;
  rflags_t                rflags;

  int                     A;
  int                     junk;
  int                     ktime;

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

        rflags = 0;
        switch (chn)
        {
            case FROLOV_BFP16_CHAN_KMAX:
            case FROLOV_BFP16_CHAN_KMIN:
                A = chn - FROLOV_BFP16_CHAN_KMAX;
                if (action == DRVA_WRITE)
                {
                    if (value < 1)      value = 1;
                    if (value > 65535) value = 65535;
                    c = value;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A, 16, &c));
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 25, &junk));

                    ReturnOne(me, FROLOV_BFP16_CHAN_KMAX_US + A);
                }
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value = c;
                break;

            case FROLOV_BFP16_CHAN_KMAX_US:
            case FROLOV_BFP16_CHAN_KMIN_US:
                A = chn - FROLOV_BFP16_CHAN_KMAX_US;
                rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 1, 1, &ktime));
                ktime &= 3;
                if (action == DRVA_WRITE)
                {
                    c = (value >> ktime) / 2500;
                    if (c < 1)     c = 1;
                    if (c > 65535) c = 65535;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A, 16, &c));
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 25, &junk));

                    ReturnOne(me, FROLOV_BFP16_CHAN_KMAX + A);
                }
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value = (c * 2500) << ktime;
                break;

            case FROLOV_BFP16_CHAN_KTIME:
                if (action == DRVA_WRITE)
                {
                    if (value < 0) value = 0;
                    if (value > 3) value = 3;
                    c = value;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 1, 17, &c));
                    ReturnOne(me, FROLOV_BFP16_CHAN_KMAX_US);
                    ReturnOne(me, FROLOV_BFP16_CHAN_KMIN_US);
                }
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 1, 1, &c));
                value = c;
                break;

            case FROLOV_BFP16_CHAN_LOOPMODE:
                if (action == DRVA_WRITE)
                {
                    c = (value & 1) | 2;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 3, 17, &c));
                }
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 3, 1, &c));
                value = c & 1;
                break;

            case FROLOV_BFP16_CHAN_NUMPERIODS:
                if (action == DRVA_WRITE)
                {
                    if (value < 0)     value = 0;
                    if (value > 65535) value = 65535;
                    c = value;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 17, &c));
                }
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 1, &c));
                value = c;
                break;

            case FROLOV_BFP16_CHAN_MOD_ENABLE:
            case FROLOV_BFP16_CHAN_MOD_DISABLE:
            case FROLOV_BFP16_CHAN_RESET_CTRS:
            case FROLOV_BFP16_CHAN_WRK_ENABLE:
            case FROLOV_BFP16_CHAN_WRK_DISABLE:
            case FROLOV_BFP16_CHAN_LOOP_START:
            case FROLOV_BFP16_CHAN_LOOP_STOP:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                {
                    A = chn - FROLOV_BFP16_CHAN_MOD_ENABLE;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A, 25, &junk));
                }
                value = 0;
                break;

            case FROLOV_BFP16_CHAN_STAT_UBS:
            case FROLOV_BFP16_CHAN_STAT_WKALWD:
            case FROLOV_BFP16_CHAN_STAT_MOD:
            case FROLOV_BFP16_CHAN_STAT_LOOP:
                rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 0, &c));
                value  = (c >> (chn - FROLOV_BFP16_CHAN_STAT_UBS)) & 1;
                break;
                
            case FROLOV_BFP16_CHAN_STAT_CURPRD:
                rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 3, 0, &c));
                c = value;
                break;
                
            default:
                value  = 0;
                rflags = CXRF_UNSUPPORTED;
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(frolov_bfp16, "Frolov BFP16",
                   NULL, NULL,
                   sizeof(frolov_bfp16_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   frolov_bfp16_init_d, NULL, frolov_bfp16_rw_p);

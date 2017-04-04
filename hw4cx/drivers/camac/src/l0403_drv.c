/*********************************************************************
*  L0102 and L0403 commutators                                       *
*  (http://www.inp.nsk.su/activity/automation/device/deviceold/devx/x0005r.html) *
*  Note: 7-th bit (R8/W8, "disable") isn't documented there.         *
*                                                                    *
*  There are 3 variants of these devices:                            *
*      1x(8->1)                                                      *
*      2x(4->1)                                                      *
*      4x(2->1)                                                      *
*  (variant is hardwired on the PCB).                                *
*                                                                    *
*  This driver's channels' layout:                                   *
*      #0: output #1                                                 *
*      #1: output #2                                                 *
*      #2: output #3                                                 *
*      #3: output #4                                                 *
*      #4: common "disable" (=1) enable (=0)                         *
*      #5: kind (1:1x(8->1), 2:2x(4->1), 3:4x(2->1)                  *
*                                                                    *
*  The driver reads type from device, but it can be overridden       *
*  via auxinfo -- "181", "241" and "421" keywords.                   *
*                                                                    *
*  History:                                                          *
*      23.07.2013: switched to direct driver API                     *
*      26.03.2007: added support for "DISABLE" and switched to drv_i *
*      02.10.2006: initial version                                   *
*********************************************************************/

#include "cxsd_driver.h"

#include "drv_i/l0403_drv_i.h"


typedef struct
{
    int       N;

    int       devkind;
} l0403_privrec_t;

static psp_paramdescr_t l0403_params[] =
{
    PSP_P_FLAG("absent", l0403_privrec_t, devkind, L0403_KIND_ABSENT, 1),
    PSP_P_FLAG("421",    l0403_privrec_t, devkind, L0403_KIND_4x2to1, 0),
    PSP_P_FLAG("181",    l0403_privrec_t, devkind, L0403_KIND_1x8to1, 0),
    PSP_P_FLAG("241",    l0403_privrec_t, devkind, L0403_KIND_2x4to1, 0),
    PSP_P_END()
};

static int l0403_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  l0403_privrec_t *me = (l0403_privrec_t*)devptr;

  int  w;

    me->N = businfo[0];

    /*!!! Should also do some checking, as was in a_l0403.h */
    /* But now we just blindly try to drop "disable" bit */
    DO_NAF(CAMAC_REF, me->N, 0,  0, &w);
    w &=~ 128;
    DO_NAF(CAMAC_REF, me->N, 0, 16, &w);

    return DEVSTATE_OPERATING;
}

static void l0403_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  l0403_privrec_t *me = (l0403_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel

  int32            value;
  rflags_t         rflags;

  int  w;

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

        if      (chn == L0403_CHAN_KIND)
        {
            value   = me->devkind;
            rflags  = 0;
        }
        else if (chn == L0403_CHAN_DISABLE)
        {
            if (action == DRVA_WRITE)
            {
                value   = (value != 0);
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                w = (w &~ 128) | (value != 0? 128 : 0);
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &w));
            }
            else
            {
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                value   = ((w & 128) != 0);
            }
        }
        else if (me->devkind == L0403_KIND_1x8to1  &&  chn == 0)
        {
            if (action == DRVA_WRITE)
            {
                value  &= 7;
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                w = (w &~ 7) | value;
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &w));
            }
            else
            {
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                value   = w & 7;
            }
        }
        else if (me->devkind == L0403_KIND_2x4to1  &&  chn >= 0  &&  chn <= 1)
        {
            if (action == DRVA_WRITE)
            {
                value  &= 3;
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                w = (w &~ (3 << (chn*2))) | (value << (chn*2));
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &w));
            }
            else
            {
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                value   = (w >> (chn*2)) & 3;
            }
        }
        else if (me->devkind == L0403_KIND_4x2to1  &&  chn >= 0  &&  chn <= 3)
        {
            if (action == DRVA_WRITE)
            {
                value  &= 1;
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                w = (w &~ (1 << chn)) | (value << chn);
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &w));
            }
            else
            {
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0,  0, &w));
                value   = (w >> chn) & 1;
            }
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


DEFINE_CXSD_DRIVER(l0403, "L0403",
                   NULL, NULL,
                   sizeof(l0403_privrec_t), l0403_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   l0403_init_d, NULL, l0403_rw_p);

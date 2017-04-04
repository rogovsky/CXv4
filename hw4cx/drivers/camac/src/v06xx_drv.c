#include "cxsd_driver.h"

#include "drv_i/v06xx_drv_i.h"


typedef struct
{
    int  N;
} v06xx_privrec_t;


static int v06xx_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  v06xx_privrec_t *me = (v06xx_privrec_t*)devptr;
  
    me->N = businfo[0];

    return DEVSTATE_OPERATING;
}

static void v06xx_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  v06xx_privrec_t *me = (v06xx_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel
  int              l;     // Line #
  
  int              c;
  int32            value;
  rflags_t         rflags;
  
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

        switch (chn)
        {
            case V06XX_CHAN_WR_N_BASE ... V06XX_CHAN_WR_N_BASE+V06XX_CHAN_WR_N_COUNT-1:
            case V06XX_CHAN_WR1:
                l = chn - V06XX_CHAN_WR_N_BASE;
                if (action == DRVA_READ)
                {
                    rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 0, &c));
                    if (chn == V06XX_CHAN_WR1)
                        value = c & 0x00FFFFFF;
                    else
                        value = ((c & (1 << l)) != 0);
                    ReturnInt32Datum(devid, chn, value, rflags);
                }
                else
                {
                    rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 0, &c));
                    ////DoDriverLog(devid, 0, "l=%-2d c=%d", l, c);
                    if (chn == V06XX_CHAN_WR1)
                        c = value & 0x00FFFFFF;
                    else
                        c = (c &~ (1 << l)) | ((value != 0) << l);
                    ////DoDriverLog(devid, 0, "     c:%d", c);
                    rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 3, 16, &c));
                    {
                      int rc;
                      
                        DO_NAF(CAMAC_REF, me->N, 0, 0, &rc);
                        ////DoDriverLog(devid, 0, "     r=%d", rc);
                    }

                    if (chn == V06XX_CHAN_WR1)
                    {
                        // Should notify about change of all channels
                        for (l = 0;  l < V06XX_CHAN_WR_N_COUNT;  l++)
                        {
                            value = (c >> l) & 1;
                            ReturnInt32Datum(devid, V06XX_CHAN_WR_N_BASE + l, value, rflags);
                        }
                        // ...including WR1
                        value = c;
                        ReturnInt32Datum(devid, V06XX_CHAN_WR1, value, rflags);
                    }
                    else
                    {
                        // Should notify about x's new value
                        value = (c >> l) & 1;
                        ReturnInt32Datum(devid, chn,            value, rflags);
                        // ...and about WR1
                        value = c;
                        ReturnInt32Datum(devid, V06XX_CHAN_WR1, value, rflags);
                    }
                }
                break;

            default:
                ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(v06xx, "V06XX",
                   NULL, NULL,
                   sizeof(v06xx_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v06xx_init_d, NULL, v06xx_rw_p);

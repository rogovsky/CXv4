#include "cxsd_driver.h"

#include "drv_i/v0628_drv_i.h"
enum {V0628_CHAN_WR_TRUE_COUNT = 16};
enum {V0628_CHAN_WR1 = V0628_CHAN_FAKE_WR1};


typedef struct
{
    int  N;
} v0628_privrec_t;


static int v0628_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  v0628_privrec_t *me = (v0628_privrec_t*)devptr;
  
    me->N = businfo[0];

    return DEVSTATE_OPERATING;
}

static void v0628_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  v0628_privrec_t *me = (v0628_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel
  int              l;     // Line #
  
  int              c;
  int              A;
  int              shift;
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
            case V0628_CHAN_WR_N_BASE ... V0628_CHAN_WR_N_BASE+V0628_CHAN_WR_TRUE_COUNT-1:
            case V0628_CHAN_WR1:
                l = chn - V0628_CHAN_WR_N_BASE;
                if (action == DRVA_READ)
                {
                    rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 0, &c));
                    if (chn == V0628_CHAN_WR1)
                        value = c & 0x00FFFFFF;
                    else
                        value = ((c & (1 << l)) != 0);
                    ReturnInt32Datum(devid, chn, value, rflags);
                }
                else
                {
                    rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 0, &c));
                    if (chn == V0628_CHAN_WR1)
                        c = value & 0x00FFFFFF;
                    else
                        c = (c &~ (1 << l)) | ((value != 0) << l);
                    rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 3, 16, &c));

                    if (chn == V0628_CHAN_WR1)
                    {
                        // Should notify about change of all channels
                        for (l = 0;  l < V0628_CHAN_WR_TRUE_COUNT;  l++)
                        {
                            value = (c >> l) & 1;
                            ReturnInt32Datum(devid, V0628_CHAN_WR_N_BASE + l, value, rflags);
                        }
                        // ...including WR1
                        value = c;
                        ReturnInt32Datum(devid, V0628_CHAN_WR1, value, rflags);
                    }
                    else
                    {
                        // Should notify about chn's new value
                        value = (c >> l) & 1;
                        ReturnInt32Datum(devid, chn,            value, rflags);
                        // ...and about WR1
                        value = c;
                        ReturnInt32Datum(devid, V0628_CHAN_WR1, value, rflags);
                    }
                }
                break;

            case V0628_CHAN_RD_N_BASE ... V0628_CHAN_RD_N_BASE+V0628_CHAN_RD_N_COUNT-1:
                A     = (chn - V0628_CHAN_RD_N_BASE) / 16 + 1;
                shift = (chn - V0628_CHAN_RD_N_BASE) % 16;
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value   = (c >> shift) & 1;
                ReturnInt32Datum(devid, chn, value, rflags);
                break;

            case V0628_CHAN_RD1:
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, 1, 0, &c));
                value   =  c & 0x0000FFFF;
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 2, 0, &c));
                value  |= (c & 0x0000FFFF) << 16;
                ReturnInt32Datum(devid, chn, value, rflags);
                break;
                
            /* Channels 16...23 go here */
            default:
                ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(v0628, "V0628",
                   NULL, NULL,
                   sizeof(v0628_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v0628_init_d, NULL, v0628_rw_p);

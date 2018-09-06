
#include "cxsd_driver.h"

#include "drv_i/g0603_drv_i.h"


typedef struct
{
    int       N;

    int       val_cache[G0603_CHAN_CODE_n_count];
    rflags_t  rfl_cache[G0603_CHAN_CODE_n_count];
} g0603_privrec_t;


static int g0603_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  g0603_privrec_t *me = (g0603_privrec_t*)devptr;

  int              i;
  
    me->N = businfo[0];
  
    for (i = 0;  i < G0603_CHAN_CODE_n_count;  i++)
    {
        me->val_cache[i] = 0;
        me->rfl_cache[i] = CXRF_CAMAC_NO_Q;
    }

    return DEVSTATE_OPERATING;
}

static void g0603_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  g0603_privrec_t *me = (g0603_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel
  int              line;

  int32            value;
  rflags_t         rflags;

  int              v;
  int              status;
  int              tries;

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

        if (chn <  G0603_CHAN_CODE_n_base  ||
            chn >= G0603_CHAN_CODE_n_base + G0603_CHAN_CODE_n_count)
        {
            value  = -1;
            rflags = CXRF_UNSUPPORTED;
        }
        else
        {
            line = chn - G0603_CHAN_CODE_n_base;
            if (action == DRVA_READ)
            {
                value  = me->val_cache[line];
                rflags = me->rfl_cache[line];
            }
            else
            {
                v = (value + 0x8000) & 0xFFFF;
                for (status = 0, tries = 0;
                     (status & CAMAC_Q) == 0  &&  tries < 7000/*~=6.7ms(PKS8_cycle)/1us(CAMAC_cycle)*/;
                     tries++)
                    status = DO_NAF(CAMAC_REF, me->N, line, 16, &v);

                rflags = status2rflags(status);
                /* Failure? */
                if (rflags & CXRF_CAMAC_NO_Q) rflags |= CXRF_IO_TIMEOUT;

                me->val_cache[line] = value;
                me->rfl_cache[line] = rflags;
            }
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(g0603, "G0603",
                   NULL, NULL,
                   sizeof(g0603_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   g0603_init_d, NULL, g0603_rw_p);

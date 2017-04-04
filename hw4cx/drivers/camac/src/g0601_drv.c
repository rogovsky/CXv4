
#include "cxsd_driver.h"

#include "drv_i/g0601_drv_i.h"


typedef struct
{
    int       N;

    int       val_cache[G0601_NUM_CHANS];
    rflags_t  rfl_cache[G0601_NUM_CHANS];
} g0601_privrec_t;

enum
{
    GZI_DEF_PERIOD = 1000,                       // 1000ms=1s
    GZI_DEF_VAL    = GZI_DEF_PERIOD * 50 / 1000  // The same as GTI code
};


static int g0601_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  g0601_privrec_t *me = (g0601_privrec_t*)devptr;

  int  i;
  int  c;
  int  junk;

  int  def_period = 1000; // 1000ms=1s
  enum {MAX_PERIOD = 65535*1000/50};

    me->N = businfo[0];
  
    if (auxinfo != NULL)
    {
        def_period = atoi(auxinfo);
        if (def_period < 0)          def_period = 0;
        if (def_period > MAX_PERIOD) def_period = MAX_PERIOD;
    }
  
    DO_NAF(CAMAC_REF, me->N, 0, 24, &junk);
  
    for (i = 0;  i < G0601_NUM_CHANS;  i++)
    {
        me->val_cache[i] = 0;
        me->rfl_cache[i] = CXRF_UNSUPPORTED;
    }

    me->val_cache[G0601_CHAN_T]    = c = def_period * 50 / 1000;
    me->rfl_cache[G0601_CHAN_T]    = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &c));
    me->val_cache[G0601_CHAN_MODE] = G0601_MODE_OFF;
    me->rfl_cache[G0601_CHAN_MODE] = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 24, &junk));

    return DEVSTATE_OPERATING;
}

static void g0601_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  g0601_privrec_t *me = (g0601_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel

  int32            value;
  rflags_t         rflags;

  int              c;
  int              junk;

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

        if (action == DRVA_WRITE)
        {
            if      (chn == G0601_CHAN_T)
            {
                me->val_cache[chn] = c = value & 0xFFFF;
                me->rfl_cache[chn] = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 16, &c));
            }
            else if (chn == G0601_CHAN_MODE)
            {
                if (value == G0601_MODE_PERIODIC  ||
                    value == G0601_MODE_DELAY)
                    me->val_cache[chn] = value;
                else
                    me->val_cache[chn] = G0601_MODE_OFF;
    
                if      (me->val_cache[chn] == G0601_MODE_PERIODIC)
                {
                    me->rfl_cache[chn] = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 24, &junk));
                    me->rfl_cache[chn] =      x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 26, &junk));
                }
                else if (me->val_cache[chn] == G0601_MODE_DELAY)
                {
                    me->rfl_cache[chn] = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 24, &junk));
                    me->rfl_cache[chn] = status2rflags(DO_NAF(CAMAC_REF, me->N, 1, 26, &junk));
                }
                else /* (val_cache[chn] == G0601_MODE_OFF) */
                {
                    me->rfl_cache[chn] = status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 24, &junk));
                }
            }
        }

        ReturnInt32Datum(devid, chn, me->val_cache[chn], me->rfl_cache[chn]);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(g0601, "G0601",
                   NULL, NULL,
                   sizeof(g0601_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   g0601_init_d, NULL, g0601_rw_p);

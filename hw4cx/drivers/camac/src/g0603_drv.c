
#include "cxsd_driver.h"
#include "advdac.h"

#include "drv_i/g0603_drv_i.h"


/*=== PKS8 specifics ===============================================*/

enum
{
    MAX_ALWD_VAL = 65535,
    MIN_ALWD_VAL = 0,
    THE_QUANT    = 0, // The "quant" itself is 1, but THE_QUANT is used in slowmo with +1 (because of kozak-encoding-specific 305...306 uncertainty)
};

enum
{
    HEARTBEAT_FREQ  = 10,
    HEARTBEAT_USECS = 1000000 / HEARTBEAT_FREQ,
};

enum
{
    DEVSPEC_CHAN_OUT_n_base      = G0603_CHAN_CODE_n_base,
    DEVSPEC_CHAN_OUT_RATE_n_base = G0603_CHAN_CODE_RATE_n_base,
    DEVSPEC_CHAN_OUT_CUR_n_base  = G0603_CHAN_CODE_CUR_n_base,
    DEVSPEC_CHAN_OUT_IMM_n_base  = G0603_CHAN_CODE_IMM_n_base,
    DEVSPEC_CHAN_OUT_TAC_n_base  = -1,
};

static inline int32 val_to_daccode_to_val(int32 val)
{
    return val;
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int              N;
    int              devid;

    int              num_isc;

    int              val_cache[G0603_CHAN_CODE_n_count];
    rflags_t         rfl_cache[G0603_CHAN_CODE_n_count];
    advdac_out_ch_t  out      [G0603_CHAN_CODE_n_count];
} g0603_privrec_t;
typedef g0603_privrec_t privrec_t;

//////////////////////////////////////////////////////////////////////

static void SendWrRq(privrec_t *me, int l, int32 val)
{
  rflags_t         rflags;

  int              v;
  int              status;
  int              tries;

    v = (val + 0x8000) & 0xFFFF;
    for (status = 0, tries = 0;
         (status & CAMAC_Q) == 0  &&  tries < 7000/*~=6.7ms(PKS8_cycle)/1us(CAMAC_cycle)*/;
         tries++)
        status = DO_NAF(CAMAC_REF, me->N, l, 16, &v);

    rflags = status2rflags(status);
    /* Failure? */
    if (rflags & CXRF_CAMAC_NO_Q) rflags |= CXRF_IO_TIMEOUT;

    me->val_cache[l] = val;
    me->rfl_cache[l] = rflags;
}

/*!!!*/
static void HandleSlowmoREADDAC_in(privrec_t *me, int l, int32 val);

static void SendRdRq(privrec_t *me, int l)
{
    HandleSlowmoREADDAC_in(me, l, me->val_cache[l]);
}

//////////////////////////////////////////////////////////////////////
#include "advdac_slowmo_kinetic_meat.h"
//////////////////////////////////////////////////////////////////////

static int g0603_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  g0603_privrec_t *me = (g0603_privrec_t*)devptr;

  int              i;
  
    me->N     = businfo[0];
    me->devid = devid;
  
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

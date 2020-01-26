#include "cxsd_driver.h"
#include "advdac.h"

#include "drv_i/v0308_drv_i.h"


/*=== V0308 (VVI) specifics ========================================*/

enum
{
    MAX_ALWD_VAL = 0xFFF,
    MIN_ALWD_VAL = 0,
    THE_QUANT    = 0, // ???
};

enum
{
    HEARTBEAT_FREQ  = 10,
    HEARTBEAT_USECS = 1000000 / HEARTBEAT_FREQ,
};

enum
{
    DEVSPEC_CHAN_OUT_n_base      = V0308_CHAN_VOLTS_base,
    DEVSPEC_CHAN_OUT_RATE_n_base = V0308_CHAN_RATE_base,
    DEVSPEC_CHAN_OUT_CUR_n_base  = V0308_CHAN_CUR_base,
    DEVSPEC_CHAN_OUT_IMM_n_base  = V0308_CHAN_IMM_base,
    DEVSPEC_CHAN_OUT_TAC_n_base  = -1,
};

static inline int32 val_to_daccode_to_val(int32 val)
{
    return val;
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int  N;
    int  devid;

    int              num_isc;

    int              val_cache[2];
    rflags_t         rfl_cache[2];
    advdac_out_ch_t  out      [2];
} v0308_privrec_t;
typedef v0308_privrec_t privrec_t;

//////////////////////////////////////////////////////////////////////
static void SendWrRq(privrec_t *me, int l, int32 val);
static void SendRdRq(privrec_t *me, int l);
#include "advdac_slowmo_kinetic_meat.h"
//////////////////////////////////////////////////////////////////////

static void v0308_hbt(int devid, void *devptr,
                      sl_tid_t tid  __attribute__((unused)),
                      void *privptr __attribute__((unused)))
{
  v0308_privrec_t *me = (v0308_privrec_t*)devptr;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, v0308_hbt, NULL);

    if (me->num_isc > 0) HandleSlowmoHbt(me);
}

static int v0308_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  v0308_privrec_t *me = (v0308_privrec_t*)devptr;
  
    me->N     = businfo[0];
    me->devid = devid;

    me->out[0].max = me->out[1].max = 3000;
    me->out[0].spd = me->out[1].spd = 20;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, v0308_hbt, NULL);

    SetChanReturnType(devid, V0308_CHAN_CUR_base, 2, IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}

static void SendWrRq(privrec_t *me, int l, int32 val)
{
  int              c;

    if (val < 0)     val = 0;
    if (val > 0xFFF) val = 0xFFF;
    c = val;
    DO_NAF(CAMAC_REF, me->N, l, 16, &c);
}

static void SendRdRq(privrec_t *me, int l)
{
  int              c;
  int32            value;
  rflags_t         rflags;

    rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, l, 0, &c));
    value   = c & 0xFFF;
    HandleSlowmoREADDAC_in(me, l, value, rflags);
}

static void v0308_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  v0308_privrec_t *me = (v0308_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel
  int              A;

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

        c      = 0;
        rflags = 0;

        A = chn & 1;
        switch (chn &~ 1)
        {
            case V0308_CHAN_VOLTS_base:
                HandleSlowmoOUT_rw     (me, chn, action, action == DRVA_WRITE? value : 0);
                goto NEXT_CHANNEL;

            case V0308_CHAN_RATE_base:
                HandleSlowmoOUT_RATE_rw(me, chn, action, action == DRVA_WRITE? value : 0);
                goto NEXT_CHANNEL;

            case V0308_CHAN_IMM_base:
                HandleSlowmoOUT_IMM_rw (me, chn, action, action == DRVA_WRITE? value : 0);
                goto NEXT_CHANNEL;

            case V0308_CHAN_CUR_base:
                rflags  = status2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value   = c & 0xFFF;
                ReturnInt32Datum(devid, chn, value, rflags);
                goto NEXT_CHANNEL;

            case V0308_CHAN_RESET_base:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    DO_NAF(CAMAC_REF, me->N, A, 10, &c);
                value = 0;
                break;

            case V0308_CHAN_SW_ON_base:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    DO_NAF(CAMAC_REF, me->N, A, 26, &c);
                break;

            case V0308_CHAN_SW_OFF_base:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    DO_NAF(CAMAC_REF, me->N, A, 24, &c);
                break;

            case V0308_CHAN_IS_ON_base:
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value = (c >> 12) & 1;
                break;

            case V0308_CHAN_OVL_U_base:
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value = (c >> 13) & 1;
                break;

            case V0308_CHAN_OVL_I_base:
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value = (c >> 14) & 1;
                break;

            case V0308_CHAN_PLRTY_base:
                rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, A, 0, &c));
                value = (c >> 15) & 1;
                break;

            default:
                value = 0;
                rflags = CXRF_UNSUPPORTED;
        }
        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(v0308, "V0308",
                   NULL, NULL,
                   sizeof(v0308_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v0308_init_d, NULL, v0308_rw_p);

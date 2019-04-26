
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

    int              plrty_shift;

    int              num_isc;

    int              val_cache[G0603_CHAN_CODE_n_count];
    rflags_t         rfl_cache[G0603_CHAN_CODE_n_count];
    advdac_out_ch_t  out      [G0603_CHAN_CODE_n_count];
} g0603_privrec_t;
typedef g0603_privrec_t privrec_t;

#define ONE_LINE_PARAMS(lab, line) \
    PSP_P_INT ("spd"lab, privrec_t, out[line].spd,  0, -65535,    +65535)
    
static psp_paramdescr_t g0603_params[] =
{
    PSP_P_FLAG("unipolar", privrec_t, plrty_shift, 0x0000, 1),
    PSP_P_FLAG("bipolar",  privrec_t, plrty_shift, 0x8000, 0),

    ONE_LINE_PARAMS( "0", 0),
    ONE_LINE_PARAMS( "1", 1),
    ONE_LINE_PARAMS( "2", 2),
    ONE_LINE_PARAMS( "3", 3),
    ONE_LINE_PARAMS( "4", 4),
    ONE_LINE_PARAMS( "5", 5),
    ONE_LINE_PARAMS( "6", 6),
    ONE_LINE_PARAMS( "7", 7),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

static void SendWrRq(privrec_t *me, int l, int32 val)
{
  rflags_t         rflags;

  int              v;
  int              status;
  int              tries;

    v = (val + me->plrty_shift) & 0xFFFF;
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
static void HandleSlowmoREADDAC_in(privrec_t *me, int l, int32 val, rflags_t rflags);

static void SendRdRq(privrec_t *me, int l)
{
    HandleSlowmoREADDAC_in(me, l, me->val_cache[l], me->rfl_cache[l]);
}

//////////////////////////////////////////////////////////////////////
#include "advdac_slowmo_kinetic_meat.h"
//////////////////////////////////////////////////////////////////////

static void g0603_hbt(int devid, void *devptr,
                      sl_tid_t tid  __attribute__((unused)),
                      void *privptr __attribute__((unused)))
{
  g0603_privrec_t *me = (g0603_privrec_t*)devptr;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, g0603_hbt, NULL);

    if (me->num_isc > 0) HandleSlowmoHbt(me);
}

static int  g0603_init_d(int devid, void *devptr,
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

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, g0603_hbt, NULL);

    SetChanReturnType(devid, G0603_CHAN_CODE_CUR_n_base, countof(me->out), IS_AUTOUPDATED_TRUSTED);

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

  int              l;     // Line #
  int32            value;

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

        if      (chn >= G0603_CHAN_CODE_n_base       &&
                 chn <  G0603_CHAN_CODE_n_base      + G0603_CHAN_CODE_n_count)
            HandleSlowmoOUT_rw     (me, chn, action, action == DRVA_WRITE? value : 0);
        else if (chn >= G0603_CHAN_CODE_RATE_n_base  &&
                 chn <  G0603_CHAN_CODE_RATE_n_base + G0603_CHAN_CODE_n_count)
            HandleSlowmoOUT_RATE_rw(me, chn, action, action == DRVA_WRITE? value : 0);
        else if (chn >= G0603_CHAN_CODE_IMM_n_base   &&
                 chn <  G0603_CHAN_CODE_IMM_n_base  + G0603_CHAN_CODE_n_count)
            HandleSlowmoOUT_IMM_rw (me, chn, action, action == DRVA_WRITE? value : 0);
        else if (chn >= G0603_CHAN_CODE_CUR_n_base   &&
                 chn <  G0603_CHAN_CODE_CUR_n_base  + G0603_CHAN_CODE_n_count)
        {
            l = chn - G0603_CHAN_CODE_CUR_n_base;
            ReturnInt32Datum(devid, chn, me->val_cache[l], me->rfl_cache[l]);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(g0603, "G0603",
                   NULL, NULL,
                   sizeof(g0603_privrec_t), g0603_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   g0603_init_d, NULL, g0603_rw_p);

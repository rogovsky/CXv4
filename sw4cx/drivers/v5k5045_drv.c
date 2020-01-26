#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/v5k5045_drv_i.h"


enum
{
    PKS_SET,
    PKS_SET_RATE,
    PKS_SET_IMM,
    PKS_SET_CUR,

    GVI_DELAY1,
    GVI_DELAY2,
    GVI_DELAY3,

    URR_RST_ILKS_B0,
    URR_RST_ILKS_B1,

    DAC_SET,

    IN1_ILK_FIRST,
    IN1_ILK_LAST = IN1_ILK_FIRST + 15,
    IN2_ILK_FIRST,
    IN2_ILK_LAST = IN2_ILK_FIRST + 15,

    SUBORD_NUMCHANS,
};
enum
{
    SODC_ILK_base  = IN1_ILK_FIRST,
    SODC_ILK_count = IN2_ILK_LAST - SODC_ILK_base + 1,

    ILK_SLOW_FIRST = 0,  ILK_SLOW_LAST = 11,
    ILK_FAST_FIRST = 12, ILK_FAST_LAST = 31,
};

enum
{
    SUBDEV_PKS,
    SUBDEV_GVI,
    SUBDEV_URR,
    SUBDEV_DAC,
    SUBDEV_IN1,
    SUBDEV_IN2,
};

static vdev_sodc_dsc_t sodc2ourc_mapping[SUBORD_NUMCHANS] =
{
    [PKS_SET]         = {"pks.code0",      VDEV_IMPR,             -1,                      SUBDEV_PKS},
    [PKS_SET_RATE]    = {"pks.code_rate0",             VDEV_TUBE, V5K5045_CHAN_HVSET_RATE, SUBDEV_PKS},
    [PKS_SET_IMM]     = {"pks.code_imm0",  VDEV_PRIV,             -1},
    [PKS_SET_CUR]     = {"pks.code_cur0",  VDEV_IMPR | VDEV_TUBE, V5K5045_CHAN_HVSET_CUR,  SUBDEV_PKS},

    [GVI_DELAY1]      = {"gvi.quant0",                 VDEV_TUBE, V5K5045_CHAN_DELAY1,     SUBDEV_GVI},
    [GVI_DELAY2]      = {"gvi.quant1",                 VDEV_TUBE, V5K5045_CHAN_DELAY2,     SUBDEV_GVI},
    [GVI_DELAY3]      = {"gvi.quant2",                 VDEV_TUBE, V5K5045_CHAN_DELAY3,     SUBDEV_GVI},

    [URR_RST_ILKS_B0] = {"urr.bit0",       VDEV_PRIV,             -1,                      SUBDEV_URR},
    [URR_RST_ILKS_B1] = {"urr.bit1",       VDEV_PRIV,             -1,                      SUBDEV_URR},

    [DAC_SET]         = {"dac.out0",       VDEV_IMPR | VDEV_TUBE, V5K5045_CHAN_PWRSET,     SUBDEV_DAC},

#define ONE_ILK_MAP(d, n) \
    [__CX_CONCATENATE(__CX_CONCATENATE(IN,d),_ILK_FIRST) + n] = \
         {"in"__CX_STRINGIZE(d)".inprb"__CX_STRINGIZE(n), VDEV_IMPR, V5K5045_CHAN_ILK_base + (d-1)*16 + n, __CX_CONCATENATE(SUBDEV_IN,d)}
#define ONE_INx_MAP(d) \
    ONE_ILK_MAP(d,  0), ONE_ILK_MAP(d,  1), ONE_ILK_MAP(d,  2), ONE_ILK_MAP(d,  3), \
    ONE_ILK_MAP(d,  4), ONE_ILK_MAP(d,  5), ONE_ILK_MAP(d,  6), ONE_ILK_MAP(d,  7), \
    ONE_ILK_MAP(d,  8), ONE_ILK_MAP(d,  9), ONE_ILK_MAP(d, 10), ONE_ILK_MAP(d, 11), \
    ONE_ILK_MAP(d, 12), ONE_ILK_MAP(d, 13), ONE_ILK_MAP(d, 14), ONE_ILK_MAP(d, 15)

    ONE_INx_MAP(1),
    ONE_INx_MAP(2),
};

static int ourc2sodc[V5K5045_NUMCHANS];

static const char *devstate_names[] =
{
    [SUBDEV_PKS] = "pks._devstate",
    [SUBDEV_GVI] = "gvi._devstate",
    [SUBDEV_URR] = "urr._devstate",
    [SUBDEV_DAC] = "dac._devstate",
    [SUBDEV_IN1] = "in1._devstate",
    [SUBDEV_IN2] = "in2._devstate",
};

enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

enum
{
    MAX_ALWD_VAL = 65535,
    MIN_ALWD_VAL = 0,

    DROP_DELTA   = 10000,
};

//--------------------------------------------------------------------

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               out_val_nominal;
    int32               out_val;
    int                 out_val_set;
    rflags_t            out_val_rflags;

    int32               c_ilks_vals[V5K5045_CHAN_ILK_count];
} privrec_t;
static psp_paramdescr_t text2cfg[] =
{
    PSP_P_INT("nominal", privrec_t, out_val_nominal, MIN_ALWD_VAL, MAX_ALWD_VAL, 0),
    PSP_P_END()
};

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    KLS_STATE_UNKNOWN   = 0,
    KLS_STATE_DETERMINE,         // 1

    KLS_STATE_INTERLOCK,         // 2
    KLS_STATE_RST_ILK_SET,       // 3
    KLS_STATE_RST_ILK_DRP,       // 4
    KLS_STATE_RST_ILK_CHK_unused,// 5; in fact, unused

    KLS_STATE_IS_OFF,            // 6

    KLS_STATE_SW_ON_UP,

    KLS_STATE_IS_ON,

    KLS_STATE_SW_OFF,

    KLS_STATE_count
};

//--------------------------------------------------------------------
static void SwchToUNKNOWN(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

///!!!    vdev_forget_all(&me->ctx);
}

static void SwchToDETERMINE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        ilk_n;
  int        ilk_eff; // EFFective value (all ORed)

    for (ilk_n = 0;  ilk_n < V5K5045_CHAN_C_ILK_count;  ilk_n++)
        ReturnInt32Datum(me->devid, V5K5045_CHAN_C_ILK_base + ilk_n,
                         me->c_ilks_vals[ilk_n], 0);

    if (!me->out_val_set)
    {
        if (me->cur[PKS_SET].flgs == 0)
        {
            me->out_val        = me->cur[PKS_SET].v.i32;
            me->out_val_rflags = 0;
        }
        else
        {
            me->out_val        = 0;
            me->out_val_rflags = CXCF_FLAG_COLOR_WEIRD;
        }
        me->out_val_set = 1; /* Yes, we do it only ONCE, since later PKS_SET may change upon our request, without any client's write */
        ReturnInt32Datum(me->devid, V5K5045_CHAN_HVSET, me->out_val, me->out_val_rflags);
    }

    ///???
    for (ilk_n = 0, ilk_eff = 0;  ilk_n < SODC_ILK_count;  ilk_n++)
        ilk_eff |= !(me->cur[SODC_ILK_base + ilk_n].v.i32); // Note: Invert

    if (ilk_eff == 0)
        vdev_set_state(&(me->ctx), KLS_STATE_IS_OFF);
    else
        vdev_set_state(&(me->ctx), KLS_STATE_INTERLOCK);
}

static void SwchToINTERLOCK(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, PKS_SET, me->out_val > DROP_DELTA? me->out_val - DROP_DELTA : 0);
}

static int  IsAlwdRST_ILK_SET(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->ctx.cur_state == KLS_STATE_INTERLOCK
        /* 26.09.2012: following added to allow [Reset] in other cases */
        || (me->ctx.cur_state >= KLS_STATE_RST_ILK_SET  &&
            me->ctx.cur_state <= KLS_STATE_RST_ILK_DRP)
        ;
}

static void Drop_c_ilks(privrec_t *me)
{
  int  x;

    for (x = 0;  x < V5K5045_CHAN_C_ILK_count;  x++)
    {
        me->c_ilks_vals[x] = 0;
        ReturnInt32Datum(me->devid,
                         V5K5045_CHAN_C_ILK_base + x,
                         me->c_ilks_vals[x], 0);
    }
}

static void SwchToRST_ILK_SET(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, URR_RST_ILKS_B0, 1);
    SndCVal(me, URR_RST_ILKS_B1, 1);
}

static void SwchToRST_ILK_DRP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    Drop_c_ilks(me);
    SndCVal(me, URR_RST_ILKS_B0, 0);
    SndCVal(me, URR_RST_ILKS_B1, 0);
}

static int  IsAlwdSW_ON_UP (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        ilk_n;
  int        ilk_eff; // EFFective value (all ORed)

    for (ilk_n = 0, ilk_eff = 0;  ilk_n < SODC_ILK_count;  ilk_n++)
        ilk_eff |= !(me->cur[SODC_ILK_base + ilk_n].v.i32); // Note: Invert

    return ilk_eff == 0;
}

static void SwchToSW_ON_UP (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int32      safe_val;

    /*SafeHVval(me)*/
    //???safe_val = 20000;
    safe_val = me->out_val > DROP_DELTA? me->out_val - DROP_DELTA : 0;
    if (safe_val > 20000) safe_val = 20000;
    /**/

    if (me->cur[PKS_SET_CUR].v.i32 < safe_val)
        SndCVal(me, PKS_SET_IMM, safe_val);
    SndCVal(me, PKS_SET, me->out_val);
}

static int  IsAlwdIS_ON    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return (me->cur[PKS_SET_CUR].v.i32 == me->out_val);
}

static int  IsAlwdSW_OFF   (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return /*!!!?!?!?!(me->cur[PKS_SET_CUR].v.i32 == me->out_val)*/1;
}

static void SwchToSW_OFF   (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, PKS_SET, me->out_val > DROP_DELTA? me->out_val - DROP_DELTA : 0);
}

static vdev_state_dsc_t state_descr[] =
{
    [KLS_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [KLS_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},

    [KLS_STATE_INTERLOCK]    = {0,       -1,                     NULL,               SwchToINTERLOCK,    NULL},

    [KLS_STATE_RST_ILK_SET]  = {500000,  KLS_STATE_RST_ILK_DRP,  IsAlwdRST_ILK_SET,  SwchToRST_ILK_SET,  NULL},
    [KLS_STATE_RST_ILK_DRP]  = {500000,  KLS_STATE_IS_OFF,       NULL,               SwchToRST_ILK_DRP,  NULL},

    [KLS_STATE_IS_OFF]       = {0,       -1,                     NULL,               NULL,               NULL},

    [KLS_STATE_SW_ON_UP]     = {1,       KLS_STATE_IS_ON,        IsAlwdSW_ON_UP,     SwchToSW_ON_UP,     NULL},

    [KLS_STATE_IS_ON]        = {0,       -1,                     IsAlwdIS_ON,        NULL,               (int[]){PKS_SET_CUR, -1}},

    [KLS_STATE_SW_OFF]       = {1,       KLS_STATE_IS_OFF,       IsAlwdSW_OFF,       SwchToSW_OFF,       NULL},
};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int v5k5045_init_mod(void)
{
  int  sn;
  int  x;

    /* Fill interesting_chan[][] mapping */
    bzero(state_important_channels, sizeof(state_important_channels));
    for (sn = 0;  sn < countof(state_descr);  sn++)
        if (state_descr[sn].impr_chans != NULL)
            for (x = 0;  state_descr[sn].impr_chans[x] >= 0;  x++)
                state_important_channels[sn][state_descr[sn].impr_chans[x]] = 1;

    /* ...and ourc->sodc mapping too */
    for (x = 0;  x < countof(ourc2sodc);  x++)
        ourc2sodc[x] = -1;
    for (x = 0;  x < countof(sodc2ourc_mapping);  x++)
        if (sodc2ourc_mapping[x].mode != 0  &&  sodc2ourc_mapping[x].ourc >= 0)
            ourc2sodc[sodc2ourc_mapping[x].ourc] = x;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void v5k5045_rw_p(int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values);
static void v5k5045_sodc_cb(int devid, void *devptr,
                            int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {V5K5045_CHAN_SWITCH_ON,     KLS_STATE_SW_ON_UP,     0,                 CX_VALUE_DISABLED_MASK},
    {V5K5045_CHAN_SWITCH_OFF,    KLS_STATE_SW_OFF,       0,                 CX_VALUE_DISABLED_MASK},
    {V5K5045_CHAN_RST_ILKS,      KLS_STATE_RST_ILK_SET,  CX_VALUE_LIT_MASK, 0},
    {V5K5045_CHAN_IS_ON,         -1,                     0,                 0},
    {-1,                         -1,                     0,                 0},
};

static int v5k5045_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;
  const char     *endp;
  char            base[100];
  size_t          base_len;

    me->devid = devid;

    // Check auxinfo presence
    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-cpoint-hierarchy name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    // Extract base name
    for (endp = p;  *endp != NULL  &&  !isspace(*endp);  endp++);
    base_len = endp - p;
    if (base_len > sizeof(base) - 1)
        base_len = sizeof(base) - 1;
    memcpy(base, p, base_len); base[base_len] = '\0';
    p = endp;

    // Parse optional parameters
    if (psp_parse(p, &p,
                  me,
                  '=', " \t", "",
                  text2cfg) != PSP_R_OK)
    {
        DoDriverLog(devid, DRIVERLOG_ERR,
                    "psp_parse(CONFIG)@init: %s",
                    psp_error());
        return -CXRF_CFG_PROBL;
    }

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = sodc2ourc_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = v5k5045_rw_p;
    me->ctx.sodc_cb        = v5k5045_sodc_cb;

    me->ctx.our_numchans             = V5K5045_NUMCHANS;
    me->ctx.chan_state_n             = V5K5045_CHAN_VDEV_STATE;
    me->ctx.state_unknown_val        = KLS_STATE_UNKNOWN;
    me->ctx.state_determine_val      = KLS_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    /*!!!SetChanRDs, SetChanReturnType*/
    SetChanRDs       (devid, V5K5045_CHAN_PWRSET,    1, 1000000.0, 0.0);
    SetChanRDs       (devid, V5K5045_CHAN_DELAY1,    3, 10.0,      0.0);
    SetChanReturnType(devid, V5K5045_CHAN_HVSET_CUR, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, V5K5045_CHAN_ILK_base,
                              V5K5045_CHAN_ILK_count,   IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, V5K5045_CHAN_C_ILK_base,
                              V5K5045_CHAN_C_ILK_count, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, V5K5045_CHAN_ILK_SLOW_ANY,
                              2,                        IS_AUTOUPDATED_YES);

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, base);
}

static void v5k5045_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void v5k5045_sodc_cb(int devid, void *devptr,
                            int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             ilk_n;

  int             s_idx;
  int             s_val;
  int             s_rcv;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == KLS_STATE_UNKNOWN) return;

    /* Perform state transitions depending on received data */
    if (sodc >= SODC_ILK_base  &&
        sodc <  SODC_ILK_base + SODC_ILK_count)
    {
        val = !val; // Invert
        ilk_n = sodc - SODC_ILK_base;
        // a. "Tube" (inverted) first
        ReturnInt32Datum(devid, V5K5045_CHAN_ILK_base + ilk_n, val, 0);
        // b. Handle cumulative interlock channels
        if (val != 0  &&  me->c_ilks_vals[ilk_n] != val)
        {
            me->c_ilks_vals[ilk_n] = val;
            ReturnInt32Datum(devid, V5K5045_CHAN_C_ILK_base + ilk_n, val, 0);
        }
        // c. Handle ORed "summary" interlocks
        // c1. Slow
        for (s_idx =  ILK_SLOW_FIRST, s_val = 0, s_rcv = 1;
             s_idx <= ILK_SLOW_LAST;  s_idx++)
        {
            s_val |= !(me->cur[SODC_ILK_base + s_idx].v.i32);
            s_rcv &=   me->cur[SODC_ILK_base + s_idx].rcvd;
        }
        if (s_rcv) ReturnInt32Datum(devid, V5K5045_CHAN_ILK_SLOW_ANY, s_val, 0);
        // c2. Fast
        for (s_idx =  ILK_FAST_FIRST, s_val = 0, s_rcv = 1;
             s_idx <= ILK_FAST_LAST;  s_idx++)
        {
            s_val |= !(me->cur[SODC_ILK_base + s_idx].v.i32);
            s_rcv &=   me->cur[SODC_ILK_base + s_idx].rcvd;
        }
        if (s_rcv) ReturnInt32Datum(devid, V5K5045_CHAN_ILK_FAST_ANY, s_val, 0);

        // d. Handle interlock itself
        if (val != 0  &&
            (// Lock-unaffected states
             me->ctx.cur_state != KLS_STATE_RST_ILK_SET  &&
             me->ctx.cur_state != KLS_STATE_RST_ILK_DRP
            )
           )
            vdev_set_state(&(me->ctx), KLS_STATE_INTERLOCK);
    }
#if 0
    else if (sodc == PKS_SET)
    {
        ReturnInt32Datum(devid, V5K5045_CHAN_HVSET, val, me->cur[PKS_SET].flgs);
    }
#endif
}

static vdev_sr_chan_dsc_t *find_sdp(int ourc)
{
  vdev_sr_chan_dsc_t *sdp;

    for (sdp = state_related_channels;
         sdp != NULL  &&  sdp->ourc >= 0;
         sdp++)
        if (sdp->ourc == ourc) return sdp;

    return NULL;
}

static void v5k5045_rw_p(int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t          *me = (privrec_t *)devptr;
  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value
  int                 sodc;
  vdev_sr_chan_dsc_t *sdp;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        sodc = ourc2sodc[chn];

        if      (sodc >= 0  &&  sodc2ourc_mapping[sodc].mode & VDEV_TUBE)
        {
            if (action == DRVA_WRITE)
                SndCVal(me, sodc, val);
            else
                if (me->cur[sodc].rcvd)
                    ReturnInt32DatumTimed(devid, chn, me->cur[sodc].v.i32,
                                          me->cur[sodc].flgs,
                                          me->cur[sodc].ts);
        }
        else if (chn == V5K5045_CHAN_HVSET)
        {
             if (action == DRVA_WRITE)
             {
                 if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                 if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                 if (me->out_val_nominal > 0  &&
                     val > me->out_val_nominal)
                     val = me->out_val_nominal;
                 me->out_val        = val;
                 me->out_val_set    = 1;
                 me->out_val_rflags = 0;

                 if (me->ctx.cur_state == KLS_STATE_IS_ON  ||
                     me->ctx.cur_state == KLS_STATE_SW_ON_UP)
                 {
                     SndCVal(me, PKS_SET, val);
                 }
                 else if (val < me->cur[PKS_SET_CUR].v.i32)
                     SndCVal(me, PKS_SET, val);
             }
             if (me->out_val_set)
                 ReturnInt32Datum(devid, chn, me->out_val, me->out_val_rflags);
        }
        else if (chn == V5K5045_CHAN_HVSET_NOMINAL)
        {
            if (action == DRVA_WRITE)
            {
                if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                me->out_val_nominal = val;
            }
            ReturnInt32Datum(devid, chn, me->out_val_nominal, 0);
        }
        else if ((sdp = find_sdp(chn)) != NULL  &&
                 sdp->state >= 0                &&
                 state_descr[sdp->state].sw_alwd != NULL)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND  &&
                state_descr[sdp->state].sw_alwd(me, me->ctx.cur_state))
                vdev_set_state(&(me->ctx), sdp->state);

            val = state_descr[sdp->state].sw_alwd(me,
                                                  me->ctx.cur_state)? sdp->enabled_val
                                                                    : sdp->disabled_val;
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == V5K5045_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), KLS_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == V5K5045_CHAN_RESET_C_ILKS)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                Drop_c_ilks(me);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == V5K5045_CHAN_IS_ON)
        {
            val = (me->ctx.cur_state >= KLS_STATE_SW_ON_UP  &&
                   me->ctx.cur_state <= KLS_STATE_IS_ON);
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == V5K5045_CHAN_VDEV_STATE)
        {
            ReturnInt32Datum(devid, chn, me->ctx.cur_state, 0);
        }
        else if (chn >= V5K5045_CHAN_C_ILK_base  &&
                 chn <  V5K5045_CHAN_C_ILK_base + V5K5045_CHAN_C_ILK_count)
        {
            ReturnInt32Datum(devid, chn,
                             me->c_ilks_vals[chn - V5K5045_CHAN_C_ILK_base],
                             0);
        }
        else if (chn == V5K5045_CHAN_HVSET_CUR)
            /* Returned from sodc_cb() */;
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(v5k5045, "VEPP5's 5045 klystron via CAMAC hardware",
                   v5k5045_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v5k5045_init_d, v5k5045_term_d, v5k5045_rw_p);

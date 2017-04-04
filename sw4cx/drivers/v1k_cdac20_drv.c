#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/v1k_cdac20_drv_i.h"


enum
{
    C20C_OUT,
    C20C_OUT_RATE,
    C20C_SW_ON,
    C20C_SW_OFF,
    C20C_RST_ILKS,

    C20C_DO_CALB_DAC,
    C20C_AUTOCALB_ONOFF,
    C20C_AUTOCALB_SECS,
    C20C_DIGCORR_MODE,

    C20C_CURRENT1M,
    C20C_VOLTAGE2M,
    C20C_CURRENT2M,
    C20C_OUTVOLTAGE1M,
    C20C_OUTVOLTAGE2M,
    C20C_ADC_DAC,
    C20C_ADC_0V,
    C20C_ADC_P10V,
    C20C_OUT_CUR,

    C20C_DIGCORR_FACTOR,

    C20C_OPR,
    C20C_ILK_OVERHEAT,
    C20C_ILK_EMERGSHT,
    C20C_ILK_CURRPROT,
    C20C_ILK_LOADOVRH,

    SUBORD_NUMCHANS
};
enum
{
    C20C_ILK_base  = C20C_ILK_OVERHEAT,
    C20C_ILK_count = C20C_ILK_LOADOVRH - C20C_ILK_base + 1,
};

static vdev_sodc_dsc_t cdac2v1k_mapping[SUBORD_NUMCHANS] =
{
    [C20C_OUT]            = {"out",            VDEV_IMPR,             -1},
    [C20C_OUT_RATE]       = {"out_rate",                   VDEV_TUBE, V1K_CDAC20_CHAN_ISET_RATE},
    [C20C_SW_ON]          = {"outrb0",         VDEV_PRIV,             -1},
    [C20C_SW_OFF]         = {"outrb1",         VDEV_PRIV,             -1},
    [C20C_RST_ILKS]       = {"outrb2",         VDEV_PRIV,             -1},

    [C20C_DO_CALB_DAC]    = {"do_calb_dac",                VDEV_TUBE, V1K_CDAC20_CHAN_DO_CALB_DAC},
    [C20C_AUTOCALB_ONOFF] = {"autocalb_onoff",             VDEV_TUBE, V1K_CDAC20_CHAN_AUTOCALB_ONOFF},
    [C20C_AUTOCALB_SECS]  = {"autocalb_secs",              VDEV_TUBE, V1K_CDAC20_CHAN_AUTOCALB_SECS},
    [C20C_DIGCORR_MODE]   = {"digcorr_mode",               VDEV_TUBE, V1K_CDAC20_CHAN_DIGCORR_MODE},

    [C20C_CURRENT1M]      = {"adc0",                       VDEV_TUBE, V1K_CDAC20_CHAN_CURRENT1M},
    [C20C_VOLTAGE2M]      = {"adc1",                       VDEV_TUBE, V1K_CDAC20_CHAN_VOLTAGE2M},
    [C20C_CURRENT2M]      = {"adc2",                       VDEV_TUBE, V1K_CDAC20_CHAN_CURRENT2M},
    [C20C_OUTVOLTAGE1M]   = {"adc3",                       VDEV_TUBE, V1K_CDAC20_CHAN_OUTVOLTAGE1M},
    [C20C_OUTVOLTAGE2M]   = {"adc4",                       VDEV_TUBE, V1K_CDAC20_CHAN_OUTVOLTAGE2M},
    [C20C_ADC_DAC]        = {"adc5",                       VDEV_TUBE, V1K_CDAC20_CHAN_ADC_DAC},
    [C20C_ADC_0V]         = {"adc6",                       VDEV_TUBE, V1K_CDAC20_CHAN_ADC_0V},
    [C20C_ADC_P10V]       = {"adc7",                       VDEV_TUBE, V1K_CDAC20_CHAN_ADC_P10V},
    [C20C_OUT_CUR]        = {"out_cur",        VDEV_IMPR | VDEV_TUBE, V1K_CDAC20_CHAN_ISET_CUR},

    [C20C_DIGCORR_FACTOR] = {"digcorr_factor",             VDEV_TUBE, V1K_CDAC20_CHAN_DIGCORR_FACTOR},

    [C20C_OPR]            = {"inprb4",         VDEV_IMPR | VDEV_TUBE, V1K_CDAC20_CHAN_OPR},
    [C20C_ILK_OVERHEAT]   = {"inprb0",         VDEV_IMPR,             -1},
    [C20C_ILK_EMERGSHT]   = {"inprb1",         VDEV_IMPR,             -1},
    [C20C_ILK_CURRPROT]   = {"inprb2",         VDEV_IMPR,             -1},
    [C20C_ILK_LOADOVRH]   = {"inprb3",         VDEV_IMPR,             -1},
};                               

static int ourc2sodc[V1K_CDAC20_NUMCHANS];

static const char *devstate_names[] = {"_devstate"};

enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

enum
{
    MAX_ALWD_VAL =   9999999,
    MIN_ALWD_VAL = -10000305
};

//--------------------------------------------------------------------

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               out_val;
    int                 out_val_set;

    int32               c_ilks_vals[V1K_CDAC20_CHAN_C_ILK_count];
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    V1K_STATE_UNKNOWN   = 0,
    V1K_STATE_DETERMINE,         // 1
    V1K_STATE_INTERLOCK,         // 2
    V1K_STATE_RST_ILK_SET,       // 3
    V1K_STATE_RST_ILK_DRP,       // 4
    V1K_STATE_RST_ILK_CHK,       // 5

    V1K_STATE_IS_OFF,            // 6

    V1K_STATE_SW_ON_ENABLE,      // 7
    V1K_STATE_SW_ON_SET_ON,      // 8
    V1K_STATE_SW_ON_DRP_ON,      // 9
    V1K_STATE_SW_ON_UP,          // 10

    V1K_STATE_IS_ON,             // 11

    V1K_STATE_SW_OFF_DOWN,       // 12
    V1K_STATE_SW_OFF_CHK_I,      // 13
    V1K_STATE_SW_OFF_CHK_E,      // 14

    V1K_STATE_count
};

//--------------------------------------------------------------------

static void SwchToUNKNOWN(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    vdev_forget_all(&me->ctx);
}

static void SwchToDETERMINE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        ilk_n;

    for (ilk_n = 0;  ilk_n < V1K_CDAC20_CHAN_C_ILK_count;  ilk_n++)
        ReturnInt32Datum(me->devid, V1K_CDAC20_CHAN_C_ILK_base + ilk_n,
                         me->c_ilks_vals[ilk_n], 0);

    if (!me->out_val_set)
    {
        me->out_val     = me->cur[C20C_OUT_CUR].v.i32;
        me->out_val_set = 1; /* Yes, we do it only ONCE, since later OUT_CUR may change upon our request, without any client's write */

        ReturnInt32Datum(me->devid, V1K_CDAC20_CHAN_ISET, me->out_val, 0);
    }

    ReturnInt32Datum(me->devid,
                     V1K_CDAC20_CHAN_ISET_CUR, me->cur[C20C_OUT_CUR].v.i32, 0);

    if (me->cur[C20C_OPR].v.i32)
        vdev_set_state(&(me->ctx), V1K_STATE_IS_ON);
    else
        vdev_set_state(&(me->ctx), V1K_STATE_IS_OFF);
}

static void SwchToINTERLOCK(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_OUT,       0);
    SndCVal(me, C20C_SW_ON,     0);
    SndCVal(me, C20C_SW_OFF,    1);
}

static int  IsAlwdRST_ILK_SET(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->ctx.cur_state == V1K_STATE_INTERLOCK
        /* 26.09.2012: following added to allow [Reset] in other cases */
        || (me->ctx.cur_state >= V1K_STATE_RST_ILK_SET  &&  me->ctx.cur_state <= V1K_STATE_RST_ILK_CHK)
        ;
}

static int  IsAlwdRST_ILK_CHK(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        sodc;

    for (sodc = C20C_ILK_base;  sodc < C20C_ILK_base + C20C_ILK_count;  sodc++)
        if (me->cur[sodc].v.i32 == 0) return 0;

    return 1;
}

static void Drop_c_ilks(privrec_t *me)
{
  int  x;

    for (x = 0;  x < V1K_CDAC20_CHAN_C_ILK_count;  x++)
    {
        me->c_ilks_vals[x] = 0;
        ReturnInt32Datum(me->devid,
                         V1K_CDAC20_CHAN_C_ILK_base + x,
                         me->c_ilks_vals[x], 0);
    }
}

static void SwchToRST_ILK_SET(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_RST_ILKS, 1);
}

static void SwchToRST_ILK_DRP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    Drop_c_ilks(me);
    SndCVal(me, C20C_RST_ILKS, 0);
}

static int  IsAlwdSW_ON_ENABLE(void *devptr, int prev_state)
{
  privrec_t *me = devptr;
  int        ilk;
  int        x;

    for (x = 0, ilk = 0;  x < C20C_ILK_count;  x++)
        ilk |= !(me->cur[C20C_ILK_base + x].v.i32); // Note: Invert

    return
        (prev_state == V1K_STATE_IS_OFF  ||
         prev_state == V1K_STATE_INTERLOCK)  &&
        me->cur[C20C_OUT_CUR].v.i32 == 0     &&
        ilk == 0;
}

static void SwchToON_ENABLE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_SW_OFF, 0);
}

static void SwchToON_SET_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_SW_ON,  1);
}

static void SwchToON_DRP_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_SW_ON,  0);
}

/* 26.09.2012: added to proceed to UP only if OPR is really on */
static int  IsAlwdSW_ON_UP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->cur[C20C_OPR].v.i32 != 0;
}

static void SwchToON_UP    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_OUT,       me->out_val);
}

static int  IsAlwdIS_ON    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return (abs(me->cur[C20C_OUT_CUR].v.i32) - me->out_val) <= 305;
}

static int  IsAlwdSW_OFF_DOWN(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    return
        /* Regular "ON" state... */
        prev_state == V1K_STATE_IS_ON         ||
        /* ...and "switching-to-on" stages */
        prev_state == V1K_STATE_SW_ON_ENABLE  ||
        prev_state == V1K_STATE_SW_ON_SET_ON  ||
        prev_state == V1K_STATE_SW_ON_DRP_ON  ||
        prev_state == V1K_STATE_SW_ON_UP      ||
        /* ...or the state bit is just "on" */
        me->cur[C20C_OPR].v.i32 != 0          ||
        /* ...or DAC current value is !=0 */
        me->cur[C20C_OUT_CUR].v.i32 != 0
        ;
}

static void SwchToSW_OFF_DOWN(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_OUT,    0);
    if (prev_state != V1K_STATE_IS_ON) SndCVal(me, C20C_SW_ON, 0);
}

static int  IsAlwdSW_OFF_CHK_I(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ////fprintf(stderr, "\tout_cur=%d\n", me->cur[C20C_OUT_CUR].v.i32);
    return me->cur[C20C_OUT_CUR].v.i32 == 0;
}

static void SwchToSW_OFF_CHK_I(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C20C_SW_ON,  0);
    SndCVal(me, C20C_SW_OFF, 1);
}

static int  IsAlwdSW_OFF_CHK_E(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ////fprintf(stderr, "\topr=%d\n", me->cur[C20C_OPR].v.i32);
    return me->cur[C20C_OPR].v.i32 == 0;
}

static vdev_state_dsc_t state_descr[] =
{
    [V1K_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [V1K_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},
    [V1K_STATE_INTERLOCK]    = {0,       -1,                     NULL,               SwchToINTERLOCK,    NULL},

    [V1K_STATE_RST_ILK_SET]  = {500000,  V1K_STATE_RST_ILK_DRP,  IsAlwdRST_ILK_SET,  SwchToRST_ILK_SET,  NULL},
    [V1K_STATE_RST_ILK_DRP]  = {500000,  V1K_STATE_RST_ILK_CHK,  NULL,               SwchToRST_ILK_DRP,  NULL},
    [V1K_STATE_RST_ILK_CHK]  = {1,       V1K_STATE_IS_OFF,       IsAlwdRST_ILK_CHK,  NULL,               (int[]){C20C_ILK_OVERHEAT, C20C_ILK_EMERGSHT, C20C_ILK_CURRPROT, C20C_ILK_LOADOVRH, -1}},

    [V1K_STATE_IS_OFF]       = {0,       -1,                     NULL,               NULL,               NULL},

    [V1K_STATE_SW_ON_ENABLE] = { 500000, V1K_STATE_SW_ON_SET_ON, IsAlwdSW_ON_ENABLE, SwchToON_ENABLE,    NULL},
    [V1K_STATE_SW_ON_SET_ON] = {1500000, V1K_STATE_SW_ON_DRP_ON, NULL,               SwchToON_SET_ON,    NULL},
    [V1K_STATE_SW_ON_DRP_ON] = { 500000, V1K_STATE_SW_ON_UP,     NULL,               SwchToON_DRP_ON,    NULL},
    [V1K_STATE_SW_ON_UP]     = {1,       V1K_STATE_IS_ON,        IsAlwdSW_ON_UP,     SwchToON_UP,        NULL},

    [V1K_STATE_IS_ON]        = {0,       -1,                     IsAlwdIS_ON,        NULL,               (int[]){C20C_OUT_CUR, -1}},

    [V1K_STATE_SW_OFF_DOWN]  = {0,       V1K_STATE_SW_OFF_CHK_I, IsAlwdSW_OFF_DOWN,  SwchToSW_OFF_DOWN,  NULL},
    [V1K_STATE_SW_OFF_CHK_I] = {0,       V1K_STATE_SW_OFF_CHK_E, IsAlwdSW_OFF_CHK_I, SwchToSW_OFF_CHK_I, (int[]){C20C_OUT_CUR, -1}},
    [V1K_STATE_SW_OFF_CHK_E] = {1,       V1K_STATE_IS_OFF,       IsAlwdSW_OFF_CHK_E, NULL,               (int[]){C20C_OPR, -1}},
};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int v1k_cdac20_init_mod(void)
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
    for (x = 0;  x < countof(cdac2v1k_mapping);  x++)
        if (cdac2v1k_mapping[x].mode != 0  &&  cdac2v1k_mapping[x].ourc >= 0)
            ourc2sodc[cdac2v1k_mapping[x].ourc] = x;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void v1k_cdac20_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values);
static void v1k_cdac20_sodc_cb(int devid, void *devptr,
                               int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {V1K_CDAC20_CHAN_SWITCH_ON,  V1K_STATE_SW_ON_ENABLE, 0,                 CX_VALUE_DISABLED_MASK},
    {V1K_CDAC20_CHAN_SWITCH_OFF, V1K_STATE_SW_OFF_DOWN,  0,                 CX_VALUE_DISABLED_MASK},
    {V1K_CDAC20_CHAN_RST_ILKS,   V1K_STATE_RST_ILK_SET,  CX_VALUE_LIT_MASK, 0},
    {V1K_CDAC20_CHAN_IS_ON,      -1,                     0,                 0},
    {-1,                         -1,                     0,                 0},
};

static int v1k_cdac20_init_d(int devid, void *devptr, 
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;

    me->devid = devid;
    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-CDAC20-device name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = cdac2v1k_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = v1k_cdac20_rw_p;
    me->ctx.sodc_cb        = v1k_cdac20_sodc_cb;

    me->ctx.our_numchans             = V1K_CDAC20_NUMCHANS;
    me->ctx.chan_state_n             = V1K_CDAC20_CHAN_V1K_STATE;
    me->ctx.state_unknown_val        = V1K_STATE_UNKNOWN;
    me->ctx.state_determine_val      = V1K_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    SetChanRDs       (devid, V1K_CDAC20_CHAN_ISET,      1, 1000000.0, 0.0);
    SetChanRDs       (devid, V1K_CDAC20_CHAN_ISET_RATE, 1, 1000000.0, 0.0);
    SetChanRDs       (devid, V1K_CDAC20_CHAN_ISET_CUR,  1, 1000000.0, 0.0);
    SetChanRDs/*!!!*/(devid, V1K_CDAC20_CHAN_RD_base,   8, 1000000.0, 0.0);
    SetChanReturnType(devid, V1K_CDAC20_CHAN_ISET_CUR,  1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, V1K_CDAC20_CHAN_ILK_base,
                              V1K_CDAC20_CHAN_ILK_count,   IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, V1K_CDAC20_CHAN_C_ILK_base,
                              V1K_CDAC20_CHAN_C_ILK_count, IS_AUTOUPDATED_TRUSTED);

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, p);
}

static void v1k_cdac20_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void v1k_cdac20_sodc_cb(int devid, void *devptr,
                               int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             ilk_n;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == V1K_STATE_UNKNOWN) return;

    /* Perform state transitions depending on received data */
    if (sodc >= C20C_ILK_base  &&
        sodc <  C20C_ILK_base + C20C_ILK_count)
    {
        val = !val; // Invert
        ilk_n = sodc - C20C_ILK_base;
        /* "Tube" (inverted) first, ... */
        ReturnInt32Datum(devid, V1K_CDAC20_CHAN_ILK_base + ilk_n, val, 0);
        if (val == 1  &&  me->c_ilks_vals[ilk_n] != val)
        {
            me->c_ilks_vals[ilk_n] = val;
            ReturnInt32Datum(devid, V1K_CDAC20_CHAN_C_ILK_base + ilk_n, val, 0);
        }
        /* ...than check */
        if (val != 0 /* Note: this is INVERTED value */  &&
            ( // Lock-unaffected states
             me->ctx.cur_state != V1K_STATE_RST_ILK_SET  &&
             me->ctx.cur_state != V1K_STATE_RST_ILK_DRP  &&
             me->ctx.cur_state != V1K_STATE_RST_ILK_CHK
            )
           )
            vdev_set_state(&(me->ctx), V1K_STATE_INTERLOCK);
    }
    else if (/* 26.09.2012: when OPR bit disappears, we should better switch off */
             sodc == C20C_OPR  &&  val == 0  &&
             (me->ctx.cur_state == V1K_STATE_SW_ON_DRP_ON  ||
              me->ctx.cur_state == V1K_STATE_SW_ON_UP      ||
              me->ctx.cur_state == V1K_STATE_IS_ON))
    {
        vdev_set_state(&(me->ctx), V1K_STATE_SW_OFF_DOWN);
    }
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

static void v1k_cdac20_rw_p(int devid, void *devptr,
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

        if      (sodc >= 0  &&  cdac2v1k_mapping[sodc].mode & VDEV_TUBE)
        {
            if (action == DRVA_WRITE)
                SndCVal(me, sodc, val);
            else
                if (me->cur[sodc].rcvd)
                    ReturnInt32DatumTimed(devid, chn, me->cur[sodc].v.i32,
                                          me->cur[sodc].flgs,
                                          me->cur[sodc].ts);
        }
        else if (chn == V1K_CDAC20_CHAN_ISET)
        {
            if (action == DRVA_WRITE)
            {
                if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                me->out_val     = val;
                me->out_val_set = 1;

                if (me->ctx.cur_state == V1K_STATE_IS_ON  ||
                    me->ctx.cur_state == V1K_STATE_SW_ON_UP)
                {
                    SndCVal(me, C20C_OUT, me->out_val);
                }
            }
            if (me->out_val_set)
                ReturnInt32Datum(devid, chn, me->out_val, 0);
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
        else if (chn == V1K_CDAC20_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), V1K_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == V1K_CDAC20_CHAN_RESET_C_ILKS)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                Drop_c_ilks(me);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == V1K_CDAC20_CHAN_IS_ON)
        {
            val = (me->ctx.cur_state >= V1K_STATE_SW_ON_ENABLE  &&
                   me->ctx.cur_state <= V1K_STATE_IS_ON);
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == V1K_CDAC20_CHAN_V1K_STATE)
        {
            ReturnInt32Datum(devid, chn, me->ctx.cur_state, 0);
        }
        else if (chn >= V1K_CDAC20_CHAN_C_ILK_base  &&
                 chn <  V1K_CDAC20_CHAN_C_ILK_base + V1K_CDAC20_CHAN_C_ILK_count)
        {
            ReturnInt32Datum(devid, chn,
                             me->c_ilks_vals[chn - V1K_CDAC20_CHAN_C_ILK_base],
                             0);
        }
        else if (chn >= V1K_CDAC20_CHAN_ILK_base  &&
                 chn <  V1K_CDAC20_CHAN_ILK_base + V1K_CDAC20_CHAN_ILK_count)
            /* Returned from sodc_cb() */;
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(v1k_cdac20, "CDAC20-controlled V1000",
                   v1k_cdac20_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v1k_cdac20_init_d, v1k_cdac20_term_d, v1k_cdac20_rw_p);

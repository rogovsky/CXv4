#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/kurrez_cac208_drv_i.h"


enum
{
    SODC_USET,
    SODC_IEXC,
    SODC_FINJ,
    SODC_POLARITY,

    SODC_UMES,
    SODC_IMES,
    SODC_PWRMES,
    SODC_FMES,
    SODC_F_TUNE_REZ,
    SODC_BIAS1,
    SODC_BIAS2,
    SODC_UINCD1,
    SODC_UREFL1,
    SODC_UINCD2,
    SODC_UREFL2,
    SODC_UDIFF,

    SODC_SW_ON,
    SODC_SW_OFF,

    SODC_IS_ON,
    SODC_IS_READY,

    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t sodc2ourc_mapping[SUBORD_NUMCHANS] =
{
    [SODC_USET]       = {"out0",   VDEV_IMPR,             -1},
    [SODC_IEXC]       = {"out1",   VDEV_IMPR,             -1},
    [SODC_FINJ]       = {"out2",               VDEV_TUBE, KURREZ_CAC208_CHAN_FINJ},
    [SODC_POLARITY]   = {"out3",   VDEV_PRIV*0+VDEV_IMPR,             -1},

    [SODC_UMES]       = {"adc0",               VDEV_TUBE, KURREZ_CAC208_CHAN_UMES},
    [SODC_IMES]       = {"adc1",               VDEV_TUBE, KURREZ_CAC208_CHAN_IMES},
    [SODC_PWRMES]     = {"adc2",               VDEV_TUBE, KURREZ_CAC208_CHAN_PWRMES},
    [SODC_FMES]       = {"adc3",               VDEV_TUBE, KURREZ_CAC208_CHAN_FMES},
    // 4
    [SODC_F_TUNE_REZ] = {"adc5",               VDEV_TUBE, KURREZ_CAC208_CHAN_F_TUNE_REZ},
    // 6
    [SODC_BIAS1]      = {"adc7",               VDEV_TUBE, KURREZ_CAC208_CHAN_BIAS1},
    [SODC_BIAS2]      = {"adc8",               VDEV_TUBE, KURREZ_CAC208_CHAN_BIAS2},
    [SODC_UINCD1]     = {"adc9",               VDEV_TUBE, KURREZ_CAC208_CHAN_UINCD1},
    [SODC_UREFL1]     = {"adc10",              VDEV_TUBE, KURREZ_CAC208_CHAN_UREFL1},
    [SODC_UINCD2]     = {"adc11",              VDEV_TUBE, KURREZ_CAC208_CHAN_UINCD2},
    [SODC_UREFL2]     = {"adc12",              VDEV_TUBE, KURREZ_CAC208_CHAN_UREFL2},
    [SODC_UDIFF]      = {"adc13",              VDEV_TUBE, KURREZ_CAC208_CHAN_UDIFF},

    [SODC_SW_ON]      = {"outrb0", VDEV_IMPR,             -1},
    [SODC_SW_OFF]     = {"outrb1", VDEV_IMPR,             -1},

    [SODC_IS_ON]      = {"inprb0", VDEV_IMPR | VDEV_TUBE, KURREZ_CAC208_CHAN_IS_ON},
    [SODC_IS_READY]   = {"inprb1", VDEV_IMPR | VDEV_TUBE, KURREZ_CAC208_CHAN_IS_READY},
};

static int ourc2sodc[KURREZ_CAC208_NUMCHANS];

static const char *devstate_names[] = {"_devstate"};

enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

/*********************************************************************
  DAC0:USET  -5V <-> 8.5kV; -5.6V <-> 9.5kV; 9.5kV is limit;
  DAC1:IEXC  -2V <-> 5.56A; ; 13A is limit;
  DAC2:FINJ  +3V <-> 25.6*; [-80*,+80*]; '+'<->left, '-'<->right
  ADC0:UMES  3.35V <-> 9.5kV
  ADC1:IMES  2.46V <-> 11.3A; Iset==-2V <-> 5.56(A|V)?
  ADC2:pMES  +1V <-> 4kW
  ADC4:FMES  1.282V <-> 25.6*
*********************************************************************/

static struct {int chn; double r;} chan_rs[] =
{
  {KURREZ_CAC208_CHAN_USET,       -1000000.0*(5.6/9.5)},
  {KURREZ_CAC208_CHAN_IEXC,       -1000000.0*(2.0/5.56)},
  {KURREZ_CAC208_CHAN_FINJ,       +1000000.0*(3.0/25.6)},
  {KURREZ_CAC208_CHAN_POLARITY,          1.0},
  {KURREZ_CAC208_CHAN_UMES,       +1000000.0*(3.35/9.5)},
  {KURREZ_CAC208_CHAN_IMES,       +1000000.0*(2.46/11.3)},
  {KURREZ_CAC208_CHAN_PWRMES,     +1000000.0*(1.0/4.0)},
  {KURREZ_CAC208_CHAN_FMES,       +1000000.0*(0.05/1/*50mv/deg*/)/*(1.282/25.6)*/},
  {KURREZ_CAC208_CHAN_MES4RSRVD,   1000000.0},
  {KURREZ_CAC208_CHAN_F_TUNE_REZ,  1000000.0*(4.5/90)},
  {KURREZ_CAC208_CHAN_MES6RSRVD,   1000000.0},
  {KURREZ_CAC208_CHAN_BIAS1,       1000000.0*(1.0/2.0)},
  {KURREZ_CAC208_CHAN_BIAS2,       1000000.0*(1.0/2.0)},
  {KURREZ_CAC208_CHAN_UINCD1,      1000000.0},
  {KURREZ_CAC208_CHAN_UREFL1,      1000000.0},
  {KURREZ_CAC208_CHAN_UINCD2,      1000000.0},
  {KURREZ_CAC208_CHAN_UREFL2,      1000000.0},
  {KURREZ_CAC208_CHAN_UDIFF,       1000000.0},

  /* Rs are identical to corresponding non-_MIN ones */
  {KURREZ_CAC208_CHAN_USET_MIN,   -1000000.0*(5.6/9.5)},
  {KURREZ_CAC208_CHAN_IEXC_MIN,   -1000000.0*(2.0/5.56)},
};

enum
{
    USET_LO = -5600000, USET_HI =  0, USET_MIN = -1768421 /* 3kV minimum */,
    IEXC_LO = -4900000, IEXC_HI =  0,
    FINJ_LO = -9370000, FINJ_HI = +9370000,
};

//--------------------------------------------------------------------

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               uset_val;
    int32               iexc_val;
    int                 uset_val_set;
    int                 iexc_val_set;

    char                errdescr[100];
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    REZ_STATE_UNKNOWN   = 0,
    REZ_STATE_DETERMINE,         // 1
    REZ_STATE_INTERLOCK,

    REZ_STATE_IS_OFF,

    REZ_STATE_SW_ON_START,
    REZ_STATE_SW_ON_SET_ON,
    REZ_STATE_SW_ON_DRP_ON,
    REZ_STATE_SW_ON_CHK_ON, /*!!!*/ // Is it really needed? Or is check in _cb() enough?
    REZ_STATE_SW_ON_CHK_UI,
    REZ_STATE_SW_ON_SET_TR, // TR -- TaRget
    REZ_STATE_SW_ON_CHKALL,

    REZ_STATE_IS_ON,

    REZ_STATE_SW_OFF_STEP1,
    REZ_STATE_SW_OFF_STEP2,

    REZ_STATE_count
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

    /* Pre-set SW_OFF bit to be 1 all the time (except when switching off) */
    SndCVal(me, SODC_SW_OFF, 1);

    /* Read settings from HW if not yet */
    if (!me->uset_val_set)
    {
        me->uset_val     = me->cur[SODC_USET].v.i32;
        me->uset_val_set = 1; /* Yes, we do it only ONCE, since later SODC value may change upon our request, without any client's write */
        ReturnInt32Datum(me->devid, KURREZ_CAC208_CHAN_USET, me->uset_val, 0);
    }
    if (!me->iexc_val_set)
    {
        me->iexc_val     = me->cur[SODC_IEXC].v.i32;
        me->iexc_val_set = 1; /* Yes, we do it only ONCE, since later SODC value may change upon our request, without any client's write */
        ReturnInt32Datum(me->devid, KURREZ_CAC208_CHAN_IEXC, me->iexc_val, 0);
    }

    /* Which state are we now in? */
    if (me->cur[SODC_IS_ON].v.i32)
        vdev_set_state(&(me->ctx), REZ_STATE_IS_ON);
    else
        vdev_set_state(&(me->ctx), REZ_STATE_IS_OFF);
}

static void SwchToINTERLOCK(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    /* Do-nothing (as for now) */
}

static int  IsAlwdSW_ON_START (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return (me->cur[SODC_IS_READY].v.i32 != 0  &&        // "READY" bit is on
            me->uset_val                 <= USET_MIN);   // 3kV minimum
}

static void SwchToSW_ON_START (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_USET, me->uset_val / 2);
    SndCVal(me, SODC_IEXC, me->iexc_val / 3);
}

static void SwchToSW_ON_SET_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_SW_ON, 1);
}

static void SwchToSW_ON_DRP_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_SW_ON, 0);
}

static void SwchToSW_ON_CHK_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    /* To be implemented */
}

static void SwchToSW_ON_CHK_UI(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    /* To be implemented */
}

static void SwchToSW_ON_SET_TR(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_USET, me->uset_val);
    SndCVal(me, SODC_IEXC, me->iexc_val);
}

static void SwchToSW_ON_CHKALL(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    /* To be implemented */
}

static int  IsAlwdSW_OFF_STEP1(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return 
        (prev_state >= REZ_STATE_SW_ON_START  &&
         prev_state <= REZ_STATE_IS_ON)  ||
        /* ...or the state bit is just "on" */
        me->cur[SODC_IS_ON].v.i32 != 0
        ;
}

static void SwchToSW_OFF_STEP1(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_SW_OFF, 0);
    SndCVal(me, SODC_USET,   0);
    SndCVal(me, SODC_IEXC,   0);
}

static void SwchToSW_OFF_STEP2(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_SW_OFF, 1);
}


static vdev_state_dsc_t state_descr[] =
{
    [REZ_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [REZ_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},
    [REZ_STATE_INTERLOCK]    = {0,       -1,                     NULL,               SwchToINTERLOCK,    NULL},

    [REZ_STATE_IS_OFF]       = {0,       -1,                     NULL,               NULL,               NULL},

    [REZ_STATE_SW_ON_START]  = { 500000, REZ_STATE_SW_ON_SET_ON, IsAlwdSW_ON_START,  SwchToSW_ON_START,  NULL},
    [REZ_STATE_SW_ON_SET_ON] = {1000000, REZ_STATE_SW_ON_DRP_ON, NULL,               SwchToSW_ON_SET_ON, NULL},
    [REZ_STATE_SW_ON_DRP_ON] = { 500000, REZ_STATE_SW_ON_CHK_ON, NULL,               SwchToSW_ON_DRP_ON, NULL},
    [REZ_STATE_SW_ON_CHK_ON] = {2000000, REZ_STATE_SW_ON_CHK_UI, NULL,               SwchToSW_ON_CHK_ON, NULL},
    [REZ_STATE_SW_ON_CHK_UI] = {1,       REZ_STATE_SW_ON_SET_TR, NULL,               SwchToSW_ON_CHK_UI, NULL},
    [REZ_STATE_SW_ON_SET_TR] = {1000000, REZ_STATE_SW_ON_CHKALL, NULL,               SwchToSW_ON_SET_TR, NULL},

    [REZ_STATE_SW_ON_CHKALL] = {0,       REZ_STATE_IS_ON,        NULL,               NULL,               NULL},

    [REZ_STATE_IS_ON]        = {0,       -1,                     NULL,               NULL,               NULL},

    [REZ_STATE_SW_OFF_STEP1] = {1000000, REZ_STATE_SW_OFF_STEP2, IsAlwdSW_OFF_STEP1, SwchToSW_OFF_STEP1, NULL},
    [REZ_STATE_SW_OFF_STEP2] = {1000000, REZ_STATE_IS_OFF,       NULL,               SwchToSW_OFF_STEP2, NULL},
};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int kurrez_cac208_init_mod(void)
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

static void kurrez_cac208_rw_p(int devid, void *devptr,
                               int action,
                               int count, int *addrs,
                               cxdtype_t *dtypes, int *nelems, void **values);
static void kurrez_cac208_sodc_cb(int devid, void *devptr,
                                  int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {KURREZ_CAC208_CHAN_SWITCH_ON,  REZ_STATE_SW_ON_START,  0, CX_VALUE_DISABLED_MASK},
    {KURREZ_CAC208_CHAN_SWITCH_OFF, REZ_STATE_SW_OFF_STEP1, 0, CX_VALUE_DISABLED_MASK},
    {-1,                            -1,                     0, 0},
};

static void SetErr(privrec_t *me, const char *str, int force)
{
  int   len;
  void *vp;

  static cxdtype_t dt_text     = CXDTYPE_TEXT;
  static int       ch_errdescr = KURREZ_CAC208_CHAN_ERRDESCR;
  static rflags_t  zero_rflags = 0;

    if (str == NULL) str = "";
    if (!force  &&  strcmp(str, me->errdescr) == 0) return;
    len = strlen(str);
    if (len > sizeof(me->errdescr) - 1)
        len = sizeof(me->errdescr) - 1;
    if (len > 0) memcpy(me->errdescr, str, len);
    me->errdescr[len] = '\0';

    ReturnDataSet(me->devid,
                  1,
                  &ch_errdescr, &dt_text, &len,
                  &vp, &zero_rflags, NULL);
}
static int kurrez_cac208_init_d(int devid, void *devptr,
                                int businfocount, int businfo[],
                                const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;
  int             x;

    me->devid = devid;
    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-CAC208-device name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = sodc2ourc_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = kurrez_cac208_rw_p;
    me->ctx.sodc_cb        = kurrez_cac208_sodc_cb;

    me->ctx.our_numchans             = KURREZ_CAC208_NUMCHANS;
    me->ctx.chan_state_n             = KURREZ_CAC208_CHAN_VDEV_STATE;
    me->ctx.state_unknown_val        = REZ_STATE_UNKNOWN;
    me->ctx.state_determine_val      = REZ_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    for (x = 0;  x < countof(chan_rs);  x++)
        SetChanRDs(devid, chan_rs[x].chn, 1, chan_rs[x].r, 0.0);
    SetChanReturnType(devid, KURREZ_CAC208_CHAN_UMES, 24, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, KURREZ_CAC208_CHAN_USET_MIN, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KURREZ_CAC208_CHAN_IEXC_MIN, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, KURREZ_CAC208_CHAN_ERRDESCR, 1, IS_AUTOUPDATED_TRUSTED);
    ReturnInt32Datum (devid, KURREZ_CAC208_CHAN_USET_MIN, USET_MIN, 0);
    SetChanReturnType(devid, KURREZ_CAC208_CHAN_PINCD1, 4, IS_AUTOUPDATED_YES);
    SetErr(me, "", 1);
#if 0
    SetChanRDs       (devid, KURREZ_CAC208_CHAN_USET,  1, -1000000.0*(5.6/9.5),  0.0);
    SetChanRDs       (devid, KURREZ_CAC208_CHAN_IEXC,  1, -1000000.0*(2.0/5.56), 0.0);
    SetChanRDs       (devid, KURREZ_CAC208_CHAN_FINJ,  1, +1000000.0*(3.0/25.6), 0.0);
    SetChanRDs       (devid, KURREZ_CAC208_CHAN_UMES,  1, +1000000.0*(3.35/9.5), 0.0);
    SetChanRDs       (devid, KURREZ_CAC208_CHAN_IMES,  1, +1000000.0*(2.46/11.3), 0.0);
    SetChanRDs       (devid, KURREZ_CAC208_CHAN_FMES,  1, +1000000.0*(1.282/25.6), 0.0);
#endif

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, p);
}

static void kurrez_cac208_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void kurrez_cac208_sodc_cb(int devid, void *devptr,
                                  int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

  int             nc;
  int             chn;
  double          v;
  double         *vp;

  static double     K_n[4]      = {4493, 4500, 4467, 4500};
  static cxdtype_t  dtype_f64   = CXDTYPE_DOUBLE;
  static int        n_1         = 1;
  static rflags_t   zero_rflags = 0;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == REZ_STATE_UNKNOWN) return;

    if      (sodc == SODC_POLARITY)
    {
        if      (val < -5000000) val = -1;
        else if (val > +5000000) val = +1;
        else                     val =  0;
        ReturnInt32DatumTimed(devid, KURREZ_CAC208_CHAN_POLARITY, val,
                              me->cur[sodc].flgs,
                              me->cur[sodc].ts);
    }
    else if (/**/
             sodc == SODC_IS_ON  &&  val == 0  &&
             (me->ctx.cur_state >= REZ_STATE_SW_ON_CHK_ON  &&
              me->ctx.cur_state <= REZ_STATE_IS_ON))
    {
        SndCVal(me, SODC_USET,   0);
        SndCVal(me, SODC_IEXC,   0);
        vdev_set_state(&(me->ctx), REZ_STATE_INTERLOCK);
    }
    else if (sodc >= SODC_UINCD1  &&
             sodc <= SODC_UREFL2)
    {
        nc  = sodc - SODC_UINCD1;
        chn = KURREZ_CAC208_CHAN_PINCD1 + nc;
        v = val / 1000000.0;
        v = v * v / 100.0 * K_n[nc];
        vp = &v;
        ReturnDataSet(devid, 1,
                      &chn, &dtype_f64, &n_1, &vp, &zero_rflags, NULL);
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

static void kurrez_cac208_rw_p(int devid, void *devptr,
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

        /* Preliminary check, for channel to be handled by TUBE code below */
        if (chn == KURREZ_CAC208_CHAN_FINJ  &&  action == DRVA_WRITE)
        {
            /* Fit into allowed range */
            if (val < FINJ_LO) val = FINJ_LO;
            if (val > FINJ_HI) val = FINJ_HI;
        }

        /* ?vdev_handle_scalar_tube_rw()? */
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
        /*---*/
        else if (chn == KURREZ_CAC208_CHAN_USET)
        {
            if (action == DRVA_WRITE)
            {
                /* Fit into allowed range */
                if (val < USET_LO) val = USET_LO;
                if (val > USET_HI) val = USET_HI;
                /* ...and store */
                me->uset_val     = val;
                me->uset_val_set = 1;

                /* Send to device if state is "ON" */
                if (me->ctx.cur_state >= REZ_STATE_SW_ON_SET_TR  &&
                    me->ctx.cur_state <= REZ_STATE_IS_ON)
                    SndCVal(me, SODC_USET, me->uset_val);
            }
            if (me->uset_val_set)
                ReturnInt32Datum(devid, chn, me->uset_val, 0);
        }
        else if (chn == KURREZ_CAC208_CHAN_IEXC)
        {
            if (action == DRVA_WRITE)
            {
                /* Fit into allowed range */
                if (val < IEXC_LO) val = IEXC_LO;
                if (val > IEXC_HI) val = IEXC_HI;
                /* ...and store */
                me->iexc_val     = val;
                me->iexc_val_set = 1;

                /* Send to device if state is "ON" */
                if (me->ctx.cur_state >= REZ_STATE_SW_ON_SET_TR  &&
                    me->ctx.cur_state <= REZ_STATE_IS_ON)
                    SndCVal(me, SODC_IEXC, me->iexc_val);
            }
            if (me->iexc_val_set)
                ReturnInt32Datum(devid, chn, me->iexc_val, 0);
        }
        else if (chn == KURREZ_CAC208_CHAN_POLARITY)
        {
            sodc = SODC_POLARITY;
            if (action == DRVA_WRITE)
            {
                if (val < 0) val = -9000000;
                if (val > 0) val = +9000000;
                SndCVal(me, sodc, val);
            }
            else if (me->cur[sodc].rcvd)
            {
                val = me->cur[sodc].v.i32;
                if      (val < -5000000) val = -1;
                else if (val > +5000000) val = +1;
                else                     val =  0;
                ReturnInt32DatumTimed(devid, chn, val,
                                      me->cur[sodc].flgs,
                                      me->cur[sodc].ts);
            }
        }
        /* ?vdev_handle_state_related_rw()? */
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
        /*---*/
        else if (chn == KURREZ_CAC208_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), REZ_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == KURREZ_CAC208_CHAN_ILK)
        {
            ReturnInt32Datum(devid, chn,
                             me->ctx.cur_state == REZ_STATE_INTERLOCK, 0);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(kurrez_cac208, "CAC208-controlled rezonator by Kurkin",
                   kurrez_cac208_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   kurrez_cac208_init_d, kurrez_cac208_term_d, kurrez_cac208_rw_p);
